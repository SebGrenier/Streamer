#include <WinSock2.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <WS2tcpip.h>
#include <fstream>
#include <sstream>

constexpr const char* server_address = "127.0.0.1";
constexpr short server_port = 12345;
constexpr int max_buffer_size = 63 * 1024;
constexpr const char* start_message = "start";
constexpr const char* end_message = "end";
constexpr const char* close_connection_message = "close";

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

int send_to_udp(SOCKET s, sockaddr_in &address, const char *buffer, unsigned buffer_size)
{
	auto size_left = buffer_size;
	int index = 0;
	int error = 0;
	while(size_left > max_buffer_size) {
		error = sendto(s, &buffer[index], max_buffer_size, 0, (struct sockaddr *)&address, sizeof(address));
		if (error == SOCKET_ERROR) {
			return error;
		}
		size_left -= max_buffer_size;
		index += max_buffer_size;
	}

	if (size_left > 0) {
		error = sendto(s, &buffer[index], size_left, 0, (struct sockaddr *)&address, sizeof(address));
	}

	return error;
}

int main(void)
{
	// Socket init
	struct sockaddr_in address;
	SOCKET sock;
	WSADATA wsa;

	// Initialise winsock
	std::cout << "Initialising Winsock..." << std::endl;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		std::cout << "Failed. Error Code : " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << "Initialised." << std::endl;

	//create socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
	{
		std::cout << "socket() failed with error code : " << WSAGetLastError() << std::endl;
		exit(EXIT_FAILURE);
	}

	//setup address structure
	memset((char *)&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(server_port);
	inet_pton(AF_INET, server_address, &address.sin_addr);

	// GLFW
	GLFWwindow* window;
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(EXIT_FAILURE);
	window = glfwCreateWindow(640, 480, "Test Window", nullptr, nullptr);
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

	GLuint depthBuffer;
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 640, 480);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Error generating framebuffer" << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Main loop
	std::vector<uint8_t> frame;
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
		//glRotatef((float)glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
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
		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(-1.0f, 1.0f, 0.f);
		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, 1.0f, 0.f);
		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, -1.0f, 0.f);
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(-1.0f, -1.0f, 0.f);
		glEnd();
		glPopAttrib();
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
		frame.clear();
		frame.resize(width * height * 3);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, (void*)frame.data());
		/*std::stringstream ss;
		ss << "image_";
		ss << count++;
		ss << ".data";
		save_image(frame, info, ss.str());*/

		send_to_udp(sock, address, start_message, strlen(start_message));
		ImageInfo info;
		info.width = width;
		info.height = height;
		info.depth = 3;
		send_to_udp(sock, address, (char*)&info, sizeof(ImageInfo));
		/*frame.clear();
		frame << std::string("0123456789");*/
		
		if (send_to_udp(sock, address, (char*)frame.data(), frame.size()) == SOCKET_ERROR)
		{
			std::cout << "send_to_udp() failed with error code : " << WSAGetLastError() << std::endl;
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		send_to_udp(sock, address, end_message, strlen(end_message));

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	send_to_udp(sock, address, end_message, strlen(end_message));
	send_to_udp(sock, address, close_connection_message, strlen(close_connection_message));

	glDeleteBuffers(1, &depthBuffer);
	glDeleteTextures(1, &texColorBuffer);
	glDeleteBuffers(1, &frame_buffer_id);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}