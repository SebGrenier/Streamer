#include "viewport_client.h"
#include "nflibs.h"

using critical_section_holder = std::lock_guard<std::mutex>;
using namespace stingray_plugin_foundation;

struct StreamingStrategy
{
	std::string format;
	std::string path;

	StreamingStrategy(const std::string &&_format, const std::string &&_path)
		: format(_format)
		, path(_path)
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

void parse_streaming_options(ConfigData *cd, cd_loc loc, StreamOptions &options)
{
	auto fetch_int = [cd, loc](auto key, auto default_value) -> int
	{
		auto handle = nfcd_object_lookup(cd, loc, key);
		if (nfcd_type(cd, handle) == CD_TYPE_NUMBER) {
			return (int)nfcd_to_number(cd, handle);
		}
		return default_value;
	};

	options.bit_rate = fetch_int("bit_rate", options.bit_rate);
	options.gop_size = fetch_int("gop_size", options.gop_size);
	options.qmin = fetch_int("qmin", options.qmin);
	options.qmax = fetch_int("qmax", options.qmax);
}

ViewportClient::ViewportClient(const EnginePluginApis &apis, const CommunicationHandlers &comm, websocketpp::connection_hdl hdl, AllocatorObject *allocator)
	: _socket_handle(hdl)
	, _closed(false)
	, _stream_opened(false)
	, _win(nullptr)
	, _allocator(allocator)
	, _alloc_api(apis.allocator_api)
	, _sc_api(apis.stream_capture_api)
	, _rb_api(apis.render_buffer_api)
	, _c_api(apis.script_api)
	, _quit(false)
	, _thread_id(nullptr)
	, _comm(comm)
	, _streamer(nullptr)
{
	_streamer = new Streamer({
		[this](uint8_t* buffer, int size) { send_binary(buffer, size); },
		[this](const std::string &msg) { info(msg); },
		[this](const std::string &msg) { warning(msg); },
		[this](const std::string &msg) { error(msg); }
	});
	_streamer->init();
}

ViewportClient::~ViewportClient()
{
	close();
	stop();

	if (_streamer != nullptr)
		delete _streamer;
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

void ViewportClient::open_stream(void *win, IdString32 buffer_name)
{
	// Do not open compression stream here as we do not know the image size
	_comm.info("opening stream");
	_win = win;
	_buffer_name = buffer_name;

	if (window_valid())
		_sc_api->enable_capture(_win, 1, (uint32_t*)(&_buffer_name));

	_stream_opened = true;
	_comm.info("finished opening stream");
}

void ViewportClient::close_stream()
{
	if (!_stream_opened)
		return;

	_comm.info("closing stream");

	if (_streamer != nullptr && _streamer->stream_opened())
		_streamer->close_stream();

	if (window_valid())
		_sc_api->disable_capture(_win, 1, (uint32_t*)&_buffer_name);

	_win = nullptr;
	_buffer_name = IdString32((unsigned)0);
	_stream_opened = false;
	_comm.info("finished closing stream");
}

void ViewportClient::resize_stream()
{
	auto win = _win;
	auto buffer = _buffer_name;
	close_stream();
	open_stream(win, buffer);
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

		// Check if it is a resize
		auto message_loc = nfcd_object_lookup(cd, root_loc, "message");
		if (nfcd_type(cd, message_loc) != CD_TYPE_NULL) {
			resize_stream();
			send_text("resize_done");
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
		auto win = _c_api->Window->get_window(window_handle);
		if (window_handle == 1 || win == nullptr)
			win = _c_api->Window->get_main_window();

		auto options_handle = nfcd_object_lookup(cd, root_loc, "options");
		if (nfcd_type(cd, options_handle) == CD_TYPE_OBJECT) {
			parse_streaming_options(cd, options_handle, _stream_options);
		}

		open_stream(win, buffer_name);

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
	_comm.send_binary(_socket_handle, buffer, size);
}

void ViewportClient::run()
{
	if (!_quit && !closed()) {

		if (!stream_opened() || !_streamer->initialized())
			return;

		if (!window_valid())
			return;

		if (_id != 0)
			return;

		SC_Buffer capture_buffer;
		auto success = _sc_api->capture_buffer(_win, _buffer_name.id(), _allocator, &capture_buffer);
		if (success) {
			auto num_byte = _rb_api->num_bits(capture_buffer.format) >> 3;
			if (!_streamer->stream_opened()) {
				_streamer->open_stream(capture_buffer.width, capture_buffer.height, num_byte, current_strategy.format, current_strategy.path, _stream_options);
			}
			_streamer->stream_frame((uint8_t*)capture_buffer.data, capture_buffer.width, capture_buffer.height, num_byte);
			_alloc_api->deallocate(_allocator, capture_buffer.data);
		}
	}
}

void ViewportClient::stop()
{
	close();
}

bool ViewportClient::window_valid() const
{
	if (_win == nullptr)
		return false;

	if (_c_api == nullptr)
		return false;

	if (!_c_api->Window->has_window((WindowPtr)_win) ||
		_c_api->Window->is_closing((WindowPtr)_win))
		return false;

	return true;
}

