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

using server = websocketpp::server<websocketpp::config::asio>;
using msg_ptr = server::message_ptr;

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

void receive_image(SOCKET s, sockaddr_in address)
{
	int address_size = sizeof(address);
	ImageInfo info;
	int total_received = 0;
	auto recv_len = recvfrom(s, (char*)&info, sizeof(ImageInfo), 0, (sockaddr *)&address, &address_size);
	int buffer_size = info.width * info.height * info.depth;

	std::cout << "Received image info: " << info << std::endl;

	char buffer[max_buffer_size];
	std::vector<uint8_t> image;
	image.reserve(buffer_size);
	while(total_received < buffer_size) {
		recv_len = recvfrom(s, buffer, max_buffer_size, 0, (sockaddr *)&address, &address_size);
		if (recv_len == SOCKET_ERROR) {
			std::cout << "recvfrom() failed with error code : " << WSAGetLastError() << std::endl;
			return;
		}

		if (_strcmpi(buffer, "end_image") == 0) {
			std::cout << "Error: received end message before end of buffer" << std::endl;
			return;
		}

		image.insert(image.end(), buffer, buffer + recv_len);
		total_received += recv_len;
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
}

void on_close(websocketpp::connection_hdl) {
	std::cout << "Close handler" << std::endl;
}

// Define a callback to handle incoming messages
void on_message(server* s, websocketpp::connection_hdl hdl, msg_ptr msg) {
	auto opcode = msg->get_opcode();

	if (opcode == websocketpp::frame::opcode::TEXT) {
		std::cout << "on_message called with hdl: " << hdl.lock().get()
			<< " and message: " << msg->get_payload()
			<< std::endl;
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

		//// Image is complete. Save it.
		//std::stringstream ss;
		//ss << "image_";
		//ss << count++;
		//ss << ".data";
		//save_image(frame, info, ss.str());
	}
}

bool validate(server *, websocketpp::connection_hdl) {
	//sleep(6);
	return true;
}

int main(int argc, char **argv)
{
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

		// Listen on port 9012
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

	return 0;

	//keep listening for data
	//while (1)
	//{

	//	//try to receive some data, this is a blocking call
	//	if ((recv_len = recvfrom(sock, buf, max_buffer_size, 0, (sockaddr *)&si_client, &slen)) == SOCKET_ERROR)
	//	{
	//		std:: cout << "recvfrom() failed with error code : " << WSAGetLastError() << std::endl;
	//		break;
	//	}

	//	//print details of the client/peer and the data received
	//	char address[INET_ADDRSTRLEN];
	//	inet_ntop(AF_INET, &si_client.sin_addr, address, INET_ADDRSTRLEN);
	//	std::cout << "Received packet from " << address << ":" << ntohs(si_client.sin_port) << std::endl;
	//	//std::cout << "Data: " << buf << std::endl;

	//	// Break out of loop if we receive a close command
	//	if (_strcmpi(buf, "close") == 0) {
	//		break;
	//	}
	//	
	//	if (_strcmpi(buf, "start_image") == 0 && listening_state == RECEIVING_STATE::LISTENING) {
	//		receive_image(sock, si_client);
	//	}
	//}

	//return 0;
}