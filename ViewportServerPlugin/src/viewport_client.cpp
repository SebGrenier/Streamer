#include "viewport_client.h"
#include "viewport_server.h"
#include "nflibs.h"
#include <d3d11.h>

using critical_section_holder = std::lock_guard<std::mutex>;
using namespace stingray_plugin_foundation;

struct StreamingStrategy
{
	std::string format;
	std::string path;
	std::string codec;

	StreamingStrategy(const std::string &&_format, const std::string &&_path)
		: format(_format)
		, path(_path)
		, codec(H264_NAME)
	{}
};

StreamingStrategy file_strategy("mp4", "../../HTML5/live/video.mp4");
StreamingStrategy rtsp_strategy("rtsp", "rtsp://127.0.0.1:54321/live.sdp");
StreamingStrategy rtmp_strategy("rtmp", "rtmp://127.0.0.1:54321/live.sdp");
StreamingStrategy mpegts_strategy("mpegts", "udp://127.0.0.1:54321");
StreamingStrategy http_strategy("hls", "../../HTML5/live/index.m3u8");
StreamingStrategy dash_strategy("stream_segment", "../../HTML5/live/video.mp4");
StreamingStrategy raw_h264_strategy("h264", "video.h264");
auto &current_strategy = raw_h264_strategy;

IdString32 buffer_name("final");

void *config_data_reallocator(void *ud, void *ptr, int osize, int nsize, const char *file, int line)
{
	if (nsize == 0) {
		free(ptr);
		return nullptr;
	}
	auto *nptr = realloc(ptr, nsize);
	return nptr;
}

void parse_streaming_options(ConfigData *cd, cd_loc loc, EncodingOptions &options)
{
	options.clear();

	auto size = nfcd_object_size(cd, loc);
	for (auto i = 0; i < size; ++i) {
		auto key = nfcd_object_key(cd, loc, i);
		auto value_loc = nfcd_object_value(cd, loc, i);
		auto value = nfcd_to_string(cd, value_loc);
		options[key] = value;
	}
}

ViewportClient::ViewportClient(ViewportServer *server, CommunicationHandlers comm, websocketpp::connection_hdl hdl, AllocatorObject *allocator)
	: _server(server)
	, _socket_handle(hdl)
	, _closed(false)
	, _stream_opened(false)
	, _win(nullptr)
	, _swap_chain_handle(0)
	, _allocator(allocator)
	, _quit(false)
	, _thread_id(nullptr)
	, _comm(comm)
	, _streamer(nullptr)
	, _nv_encode_session(nullptr)
{
	_streamer = new Streamer({
		[this](uint8_t* buffer, int size) { send_binary(buffer, size); },
		[this](const std::string &msg) { info(msg); },
		[this](const std::string &msg) { warning(msg); },
		[this](const std::string &msg) { error(msg); }
	});
	_streamer->init();

	_nv_encode_session = new NVEncodeSession(_server->apis().nvenc_api, &_comm);
}

ViewportClient::~ViewportClient()
{
	close();
	stop();

	if (_streamer != nullptr)
		delete _streamer;

	if (_nv_encode_session != nullptr)
		delete _nv_encode_session;
}

void ViewportClient::close()
{
	if (_closed)
		return;

	if (_stream_opened)
		close_stream();

	if (_streamer != nullptr)
		_streamer->shutdown();

	_closed = true;
}

void ViewportClient::open_stream(void *win, unsigned sch, IdString32 buffer_name)
{
	// Do not open compression stream here as we do not know the image size
	_comm.info("opening stream");
	_win = win;
	_swap_chain_handle = sch;
	_buffer_name = buffer_name;

	if (window_valid())
		_server->apis().stream_capture_api->enable_capture(_win, 1, (uint32_t*)(&_buffer_name));

	auto device = (ID3D11Device*)_server->apis().render_interface_api->device();
	auto render_target = _server->apis().render_interface_api->render_target(_swap_chain_handle, 0, 0);
	ID3D11Resource *texture = nullptr;
	render_target.render_target_view->GetResource(&texture);

	auto *sci = _server->get_swap_chain_info(sch);
	if (sci == nullptr) {
		error("invalid swapchain");
		return;
	}

	_stream_options["width"] = std::to_string(sci->width);
	_stream_options["height"] = std::to_string(sci->height);

	auto success = _nv_encode_session->open(device, NV_ENC_DEVICE_TYPE_DIRECTX, texture, _stream_options);
	if (success != NV_ENC_SUCCESS) {
		error("Failed to initialize encoding session");
		return;
	}

	_stream_opened = true;
	_comm.info("finished opening stream");
}

void ViewportClient::close_stream()
{
	if (!_stream_opened)
		return;

	_nv_encode_session->close();

	_comm.info("closing stream");

	if (_streamer != nullptr && _streamer->stream_opened())
		_streamer->close_stream();

	if (window_valid())
		_server->apis().stream_capture_api->disable_capture(_win, 1, (uint32_t*)&_buffer_name);

	_win = nullptr;
	_swap_chain_handle = 0;
	_buffer_name = IdString32((unsigned)0);
	_stream_opened = false;
	_comm.info("finished closing stream");
}

void ViewportClient::resize_stream()
{
	auto win = _win;
	auto sch = _swap_chain_handle;
	auto buffer = _buffer_name;
	close_stream();
	open_stream(win, sch, buffer);
}

void ViewportClient::handle_message(websocketpp::connection_hdl hdl, msg_ptr msg)
{
	auto opcode = msg->get_opcode();

	if (opcode == websocketpp::frame::opcode::TEXT) {
		std::stringstream ss;
		ss << "on_message called with hdl: " << hdl.lock().get()
			<< " and message: " << msg->get_payload();
		info(ss.str());

		static struct nfjp_Settings s = { 1, 1, 1, 1, 1, 1 };
		auto *cd = nfcd_make(config_data_reallocator, nullptr, 0, 0);

		auto *error_code = nfjp_parse_with_settings(msg->get_payload().c_str(), &cd, &s);
		if (error_code) {
			error("Failed to parse message");
			return;
		}

		auto root_loc = nfcd_root(cd);

		auto parse_codec = [this]()
		{
			auto it = _stream_options.find("codec");
			if (it != _stream_options.end()) {
				current_strategy.codec = it->second;

				// Remove the codec from the options as it is not a real supported options for a codec.
				_stream_options.erase(it);
			}
		};

		auto parse_options = [cd, root_loc, &parse_codec, this]()
		{
			auto options_handle = nfcd_object_lookup(cd, root_loc, "options");
			if (nfcd_type(cd, options_handle) == CD_TYPE_OBJECT) {
				parse_streaming_options(cd, options_handle, _stream_options);
				parse_codec();
			}
		};

		// Check if it is a resize
		auto message_loc = nfcd_object_lookup(cd, root_loc, "message");
		if (nfcd_type(cd, message_loc) != CD_TYPE_NULL) {
			auto type = nfcd_to_string(cd, message_loc);
			if (strcmp(type, "resize") == 0) {
				resize_stream();
				send_text("resize_done");
			} else if (strcmp(type, "options") == 0) {
				parse_options();
				resize_stream();
			}
			return;
		}

		// Get viewport information to open stream
		auto handle_loc = nfcd_object_lookup(cd, root_loc, "handle");
		if (nfcd_type(cd, handle_loc) == CD_TYPE_NULL) {
			error("Missing handle");
			return;
		}
		auto id_loc = nfcd_object_lookup(cd, root_loc, "id");
		if (nfcd_type(cd, id_loc) == CD_TYPE_NUMBER) {
			_id = (int)nfcd_to_number(cd, id_loc);
		}
		auto window_handle = (unsigned)nfcd_to_number(cd, handle_loc);
		auto win = _server->apis().script_api->Window->get_window(window_handle);
		if (window_handle == 1 || win == nullptr)
			win = _server->apis().script_api->Window->get_main_window();

		auto new_window_handle = _server->apis().script_api->Window->id(win);
		auto sci = _server->get_swap_chain_for_window((void*)new_window_handle);

		parse_options();
		open_stream(win, sci->handle, buffer_name);

		nfcd_free(cd);
	}
}

void ViewportClient::handle_close(websocketpp::connection_hdl hdl)
{
	info("Client handle_close");
	stop();
}

void ViewportClient::handle_fail(websocketpp::connection_hdl hdl)
{
	info("Client handle_fail");
	stop();
}

void ViewportClient::info(const std::string &message)
{
	_comm.info(message);
}

void ViewportClient::warning(const std::string &message)
{
	_comm.warning(message);
}

void ViewportClient::error(const std::string &message)
{
	_comm.error(message);
}

void ViewportClient::send_text(const std::string &message)
{
	_comm.send_text(_socket_handle, message);
}

void ViewportClient::send_binary(void* buffer, int size)
{
	_server->apis().profiler_api->profile_start("ViewportClient:send_binary");
	_comm.send_binary(_socket_handle, buffer, size);
	_server->apis().profiler_api->profile_stop();
}

void ViewportClient::run()
{
	_server->apis().profiler_api->profile_start("ViewportServer:run_all_clients");
	if (!_quit && !closed()) {

		if (!stream_opened() || !_streamer->initialized())
			return;

		if (!window_valid())
			return;

		if (_id != 0)
			return;

		SC_Buffer capture_buffer;
		_server->apis().profiler_api->profile_start("ViewportServer:capture_buffer");
		auto success = _server->apis().stream_capture_api->capture_buffer(_win, _buffer_name.id(), _allocator, &capture_buffer);
		_server->apis().profiler_api->profile_stop();
		if (success) {
			auto num_byte = _server->apis().render_buffer_api->num_bits(capture_buffer.format) >> 3;
			if (!_streamer->stream_opened()) {
				_streamer->open_stream(capture_buffer.width, capture_buffer.height, num_byte, current_strategy.format, current_strategy.codec, _stream_options);
			}
			_server->apis().profiler_api->profile_start("ViewportServer:stream_frame");
			_streamer->stream_frame((uint8_t*)capture_buffer.data, capture_buffer.width, capture_buffer.height, num_byte);
			_server->apis().profiler_api->profile_stop();
			_server->apis().allocator_api->deallocate(_allocator, capture_buffer.data);
		}
	}
	_server->apis().profiler_api->profile_stop();
}

void ViewportClient::stop()
{
	close();
}

void ViewportClient::render(unsigned sch)
{

}

bool ViewportClient::window_valid() const
{
	if (_win == nullptr)
		return false;

	if (_server->apis().script_api== nullptr)
		return false;

	if (!_server->apis().script_api->Window->has_window((WindowPtr)_win) ||
		_server->apis().script_api->Window->is_closing((WindowPtr)_win))
		return false;

	return true;
}

