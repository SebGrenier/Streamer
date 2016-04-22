#include "streamer.h"

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <functional>

// The ASIO_STANDALONE define is necessary to use the standalone version of Asio.
// Remove if you are using Boost Asio.
//#define ASIO_STANDALONE

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

constexpr int max_buffer_size = 63 * 1024;
constexpr const char* server_address = "127.0.0.1";
constexpr short server_port = 12345;
int count = 0;

Streamer streamer;

using server = websocketpp::server<websocketpp::config::asio>;
using msg_ptr = server::message_ptr;

struct StreamingStrategy
{
	std::string format;
	std::string path;

	StreamingStrategy(const std::string &&_format, const std::string &&_path)
		: format(_format)
		, path(_path)
	{}
};

StreamingStrategy file_strategy("mp4", "test.mp4");
StreamingStrategy rtsp_strategy("rtsp", "rtsp://127.0.0.1:54321/live.sdp");
StreamingStrategy mpegts_strategy("mpegts", "udp://127.0.0.1:54321");

struct ImageInfo
{
	unsigned width;
	unsigned height;
	short depth;
};

std::ostream& operator << (std::ostream &stream, const ImageInfo &image_info)
{
	stream << "(" << image_info.width << "x" << image_info.height << "x" << image_info.depth << ")";
	return stream;
}

std::ostream& operator << (std::ostream &stream, const std::vector<char> &vec)
{
	for (auto &c : vec) {
		stream << c;
	}
	return stream;
}

void save_image(const std::vector<uint8_t> image, const ImageInfo &image_info, const std::string &filename)
{
	std::ofstream file;
	file.open(filename, std::ios::out | std::ios::binary);

	if (file.is_open()) {
		file.write((char*)image.data(), image.size());
		file.close();
	}
}

void on_http(server* s, websocketpp::connection_hdl hdl) {
	server::connection_ptr con = s->get_con_from_hdl(hdl);

	std::string res = con->get_request_body();

	std::stringstream ss;
	ss << "got HTTP request with " << res.size() << " bytes of body data.";

	con->set_body(ss.str());
	con->set_status(websocketpp::http::status_code::ok);
}

void on_fail(server* s, websocketpp::connection_hdl hdl) {
	server::connection_ptr con = s->get_con_from_hdl(hdl);

	std::cout << "Fail handler: " << con->get_ec() << " " << con->get_ec().message() << std::endl;

	if (streamer.stream_opened()) {
		streamer.close_stream();
	}
}

void on_close(websocketpp::connection_hdl) {
	std::cout << "Close handler" << std::endl;
	if (streamer.stream_opened()) {
		streamer.close_stream();
	}
}

// Define a callback to handle incoming messages
void on_message(server* s, websocketpp::connection_hdl hdl, msg_ptr msg) {
	auto opcode = msg->get_opcode();

	if (opcode == websocketpp::frame::opcode::TEXT) {
		/*std::cout << "on_message called with hdl: " << hdl.lock().get()
			<< " and message: " << msg->get_payload()
			<< std::endl;*/
	} else if (opcode == websocketpp::frame::opcode::BINARY) {
		auto &payload = msg->get_payload();
		if (payload.size() <= sizeof(ImageInfo)) {
			std::cout << "Error receiving frame: the size of the buffer is too small" << std::endl;
			return;
		}

		ImageInfo info;
		memcpy_s((void*)&info, sizeof(info), payload.data(), sizeof(info));
		auto frame_size = info.width * info.height * info.depth;
		std::vector<uint8_t> frame;
		frame.resize(frame_size);
		memcpy_s(frame.data(), frame_size, payload.data() + sizeof(info), payload.size() - sizeof(info));

		StreamingInfo stream_info;
		stream_info.width = info.width;
		stream_info.height = info.height;
		if (streamer.stream_opened() && stream_info != streamer.streaming_info()) {
			streamer.close_stream();
		}

		if (!streamer.stream_opened()) {
			streamer.open_stream(info.width, info.height, info.depth, mpegts_strategy.format, mpegts_strategy.path);
		}

		if (streamer.stream_opened()) {
			streamer.stream_frame(frame.data(), info.width, info.height, info.depth);
		}
	}
}

bool validate(server *, websocketpp::connection_hdl) {
	//sleep(6);
	return true;
}

int main(int argc, char **argv)
{
	streamer.init();

	server serv;
	try {
		// Set logging settings
		serv.set_access_channels(websocketpp::log::alevel::all);
		serv.clear_access_channels(websocketpp::log::alevel::frame_payload);

		// Initialize ASIO
		serv.init_asio();
		serv.set_reuse_addr(true);

		// Register our message handler
		serv.set_message_handler(bind(&on_message, &serv, std::placeholders::_1, std::placeholders::_2));

		serv.set_http_handler(bind(&on_http, &serv, std::placeholders::_1));
		serv.set_fail_handler(bind(&on_fail, &serv, std::placeholders::_1));
		serv.set_close_handler(&on_close);

		serv.set_validate_handler(bind(&validate, &serv, std::placeholders::_1));

		// Listen on port
		serv.listen(server_port);

		// Start the server accept loop
		serv.start_accept();

		// Start the ASIO io_service run loop
		serv.run();
	}
	catch (const std::exception & e) {
		std::cout << e.what() << std::endl;
	}
	catch (websocketpp::lib::error_code e) {
		std::cout << e.message() << std::endl;
	}
	catch (...) {
		std::cout << "other exception" << std::endl;
	}

	if (streamer.stream_opened()) {
		streamer.close_stream();
	}
	streamer.shutdown();
	return 0;
}