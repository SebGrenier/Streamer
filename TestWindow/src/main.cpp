#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <WS2tcpip.h>
#include <fstream>
#include <sstream>

// The ASIO_STANDALONE define is necessary to use the standalone version of Asio.
// Remove if you are using Boost Asio.
//#define ASIO_STANDALONE

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>

using client = websocketpp::client<websocketpp::config::asio>;
using msg_ptr = client::message_ptr;

constexpr const char* server_address = "127.0.0.1";
constexpr short server_port = 12345;
constexpr int max_buffer_size = 63 * 1024;

constexpr int width_start = 1280;
constexpr int height_start = 720;
constexpr int bitdepth = 3;

std::vector<uint8_t>& operator << (std::vector<uint8_t> &vec, std::string &string)
{
	for(auto &c : string) {
		vec.push_back(c);
	}
	return vec;
}

struct ImageInfo
{
	unsigned width;
	unsigned height;
	short depth;
};

void save_image(const std::vector<uint8_t> image, const ImageInfo &image_info, const std::string &filename)
{
	std::ofstream file;
	file.open(filename, std::ios::out | std::ios::binary);

	if (file.is_open()) {
		file.write((char*)image.data(), image.size());
		file.close();
	}
}

void save_image(const uint8_t *image, const ImageInfo &image_info, const std::string &filename)
{
	std::ofstream file;
	file.open(filename, std::ios::out | std::ios::binary);

	if (file.is_open()) {
		file.write((char*)image, image_info.width * image_info.height * image_info.depth);
		file.close();
	}
}

static void error_callback(int error, const char* description)
{
	fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

class ConnectionMetadata
{
public:
	typedef websocketpp::lib::shared_ptr<ConnectionMetadata> ptr;

	ConnectionMetadata(int id, websocketpp::connection_hdl hdl, std::string uri)
		: _id(id)
		, _hdl(hdl)
		, _status("Connecting")
		, _uri(uri)
		, _server("N/A")
	{}

	void on_open(client * c, websocketpp::connection_hdl hdl)
	{
		_status = "Open";

		client::connection_ptr con = c->get_con_from_hdl(hdl);
		_server = con->get_response_header("Server");
	}

	void on_fail(client * c, websocketpp::connection_hdl hdl)
	{
		_status = "Failed";

		client::connection_ptr con = c->get_con_from_hdl(hdl);
		_server = con->get_response_header("Server");
		_error_reason = con->get_ec().message();
	}

	void on_close(client * c, websocketpp::connection_hdl hdl)
	{
		_status = "Closed";
		client::connection_ptr con = c->get_con_from_hdl(hdl);
		std::stringstream s;
		s << "close code: " << con->get_remote_close_code() << " ("
			<< websocketpp::close::status::get_string(con->get_remote_close_code())
			<< "), close reason: " << con->get_remote_close_reason();
		_error_reason = s.str();
	}

	websocketpp::connection_hdl get_hdl() const
	{
		return _hdl;
	}

	int get_id() const
	{
		return _id;
	}

	std::string get_status() const
	{
		return _status;
	}

	friend std::ostream & operator<< (std::ostream & out, ConnectionMetadata const & data);
private:
	int _id;
	websocketpp::connection_hdl _hdl;
	std::string _status;
	std::string _uri;
	std::string _server;
	std::string _error_reason;
};

std::ostream & operator<< (std::ostream & out, ConnectionMetadata const & data)
{
	out << "> URI: " << data._uri << "\n"
		<< "> Status: " << data._status << "\n"
		<< "> Remote Server: " << (data._server.empty() ? "None Specified" : data._server) << "\n"
		<< "> Error/close reason: " << (data._error_reason.empty() ? "N/A" : data._error_reason);

	return out;
}

class StreamClient
{
public:
	StreamClient()
		: _next_id(0)
	{
		_client.clear_access_channels(websocketpp::log::alevel::all);
		_client.clear_error_channels(websocketpp::log::elevel::all);

		_client.init_asio();
		_client.start_perpetual();

		_thread.reset(new websocketpp::lib::thread(&client::run, &_client));
	}

	~StreamClient()
	{
		_client.stop_perpetual();

		for (connection_list::const_iterator it = _connection_list.begin(); it != _connection_list.end(); ++it) {
			if (it->second->get_status() != "Open") {
				// Only close open connections
				continue;
			}

			std::cout << "> Closing connection " << it->second->get_id() << std::endl;

			websocketpp::lib::error_code ec;
			_client.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
			if (ec) {
				std::cout << "> Error closing connection " << it->second->get_id() << ": "
					<< ec.message() << std::endl;
			}
		}

		_thread->join();
	}

	int connect(std::string const & uri)
	{
		websocketpp::lib::error_code ec;

		client::connection_ptr con = _client.get_connection(uri, ec);

		if (ec) {
			std::cout << "> Connect initialization error: " << ec.message() << std::endl;
			return -1;
		}

		int new_id = _next_id++;
		ConnectionMetadata::ptr metadata_ptr(new ConnectionMetadata(new_id, con->get_handle(), uri));
		_connection_list[new_id] = metadata_ptr;

		con->set_open_handler(websocketpp::lib::bind(
			&ConnectionMetadata::on_open,
			metadata_ptr,
			&_client,
			websocketpp::lib::placeholders::_1
			));
		con->set_fail_handler(websocketpp::lib::bind(
			&ConnectionMetadata::on_fail,
			metadata_ptr,
			&_client,
			websocketpp::lib::placeholders::_1
			));
		con->set_close_handler(websocketpp::lib::bind(
			&ConnectionMetadata::on_close,
			metadata_ptr,
			&_client,
			websocketpp::lib::placeholders::_1
			));

		_client.connect(con);

		return new_id;
	}

	void close(int id, websocketpp::close::status::value code, std::string reason)
	{
		websocketpp::lib::error_code ec;

		connection_list::iterator metadata_it = _connection_list.find(id);
		if (metadata_it == _connection_list.end()) {
			std::cout << "> No connection found with id " << id << std::endl;
			return;
		}

		_client.close(metadata_it->second->get_hdl(), code, reason, ec);
		if (ec) {
			std::cout << "> Error initiating close: " << ec.message() << std::endl;
		}
	}

	ConnectionMetadata::ptr get_metadata(int id) const
	{
		connection_list::const_iterator metadata_it = _connection_list.find(id);
		if (metadata_it == _connection_list.end()) {
			return ConnectionMetadata::ptr();
		}
		else {
			return metadata_it->second;
		}
	}

	void send_binary(int id, void* data, int size)
	{
		auto metadata_it = _connection_list.find(id);
		if (metadata_it == _connection_list.end()) {
			std::cout << "> No connection found with id " << id << std::endl;
			return;
		}

		_client.send(metadata_it->second->get_hdl(), data, size, websocketpp::frame::opcode::BINARY);
	}

	void send_text(int id, const std::string &message)
	{
		auto metadata_it = _connection_list.find(id);
		if (metadata_it == _connection_list.end()) {
			std::cout << "> No connection found with id " << id << std::endl;
			return;
		}

		_client.send(metadata_it->second->get_hdl(), message, websocketpp::frame::opcode::TEXT);
	}

	void send_image(int id, const ImageInfo &info, const std::vector<uint8_t> &frame)
	{
		auto buffer_size = sizeof(ImageInfo) + frame.size();
		auto *buffer = new uint8_t[buffer_size];
		memcpy_s(buffer, buffer_size, (void*)&info, sizeof(info));
		memcpy_s(buffer + sizeof(info), buffer_size - sizeof(info), frame.data(), frame.size());
		send_binary(id, buffer, buffer_size);
		delete[] buffer;
	}

private:
	using connection_list = std::map<int, ConnectionMetadata::ptr>;

	client _client;
	websocketpp::lib::shared_ptr<websocketpp::lib::thread> _thread;

	connection_list _connection_list;
	int _next_id;
};

int main(void)
{
	StreamClient c;

	std::stringstream ss;
	ss << "ws://" << server_address << ":" << server_port;
	auto id = c.connect(ss.str());
	if (id < 0) {
		return -1;
	}

	// GLFW
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	window = glfwCreateWindow(width_start, height_start, "Test Window", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, key_callback);

	glewInit();

	GLuint frame_buffer_id;
	glGenFramebuffers(1, &frame_buffer_id);
	glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);

	GLuint texColorBuffer;
	glGenTextures(1, &texColorBuffer);
	glBindTexture(GL_TEXTURE_2D, texColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_start, height_start, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

	GLuint depthBuffer;
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width_start, height_start);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error generating framebuffer" << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Main loop
	std::vector<uint8_t> frame;
	ImageInfo info;
	int count = 0;
	while (!glfwWindowShouldClose(window))
	{
		float ratio;
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		ratio = width / (float)height;

		// Draw into the frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef((float)glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
		glBegin(GL_TRIANGLES);
		glColor3f(1.f, 0.f, 0.f);
		glVertex3f(-0.6f, -0.4f, 0.f);
		glColor3f(0.f, 1.f, 0.f);
		glVertex3f(0.6f, -0.4f, 0.f);
		glColor3f(0.f, 0.f, 1.f);
		glVertex3f(0.f, 0.6f, 0.f);
		glEnd();
		glPopAttrib();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Draw the texture
		glClear(GL_COLOR_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texColorBuffer);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glEnable(GL_TEXTURE_2D);
		glViewport(0, 0, width, height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(-1.0f, 1.0f, 0.f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, 1.0f, 0.f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, -1.0f, 0.f);
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-1.0f, -1.0f, 0.f);
		glEnd();
		glPopAttrib();
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
		frame.clear();
		frame.resize(width * height * bitdepth);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, (void*)frame.data());
		info.width = width;
		info.height = height;
		info.depth = bitdepth;

		std::stringstream sss;
		sss << "Frame number : " << count++;
		c.send_text(id, sss.str());

		c.send_image(id, info, frame);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteBuffers(1, &depthBuffer);
	glDeleteTextures(1, &texColorBuffer);
	glDeleteBuffers(1, &frame_buffer_id);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}