#include "viewport_client.h"
#include "viewport_server.h"
#include "nflibs.h"
#include <engine_plugin_api/plugin_api.h>

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
	, _mode(CaptureMode::STREAMED_COMPRESSED_H264)
	, _allocator(allocator)
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
		_server->apis().stream_capture_api->enable_capture(_win, 1, (uint32_t*)(&_buffer_name));


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
		_server->apis().stream_capture_api->disable_capture(_win, 1, (uint32_t*)&_buffer_name);

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
		auto type_loc = nfcd_object_lookup(cd, root_loc, "type");
		if (nfcd_type(cd, type_loc) == CD_TYPE_NULL) {
			error("missing type");
			return;
		}
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

		_mode = (CaptureMode)((int)nfcd_to_number(cd, type_loc));

		parse_options();
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

		SC_Buffer capture_buffer;
		_server->apis().profiler_api->profile_start("ViewportServer:capture_buffer");
		auto success = _server->apis().stream_capture_api->capture_buffer(_win, _buffer_name.id(), _allocator, &capture_buffer);
		_server->apis().profiler_api->profile_stop();
		if (success) {
			auto num_byte = _server->apis().render_buffer_api->num_bits(capture_buffer.format) >> 3;
			_server->apis().profiler_api->profile_start("ViewportServer:stream_frame");

			// TODO: Refactor this
			switch (_mode) {
			case CaptureMode::STREAMED_COMPRESSED_H264:
				if (!_streamer->stream_opened()) {
					_streamer->open_stream(capture_buffer.width, capture_buffer.height, num_byte, current_strategy.format, current_strategy.codec, _stream_options);
				}
				_streamer->stream_frame((uint8_t*)capture_buffer.data, capture_buffer.width, capture_buffer.height, num_byte);
				break;
			case CaptureMode::STREAMED_UNCOMPRESSED: {
				struct BinaryDataHeader {
					unsigned int size;
					unsigned int width;
					unsigned int height;
					unsigned int bpp;
					unsigned int color_buffer_size;
					unsigned int compressed_color_buffer_size;
					unsigned int depth_buffer_size;
				} bd;

				const auto frame_size = capture_buffer.width * capture_buffer.height * num_byte;
				const auto binary_data_size = sizeof(BinaryDataHeader) + frame_size;
				unsigned char *buffer = new unsigned char[binary_data_size];
				bd.size = sizeof(BinaryDataHeader);
				bd.width = capture_buffer.width;
				bd.height = capture_buffer.height;
				bd.bpp = num_byte;
				bd.color_buffer_size = frame_size;
				bd.compressed_color_buffer_size = frame_size;
				bd.depth_buffer_size = 0;


				memmove(buffer, &bd, sizeof(BinaryDataHeader));
				memmove(buffer + sizeof(BinaryDataHeader), capture_buffer.data, frame_size);

				send_binary(buffer, binary_data_size);
				delete[] buffer;
				break;
			}
			default:
				break;
			}

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

