#include <WinSock2.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <WS2tcpip.h>

constexpr const char* server_address = "127.0.0.1";
constexpr short server_port = 12345;
constexpr int max_buffer_size = 63 * 1024;
constexpr const char* start_message = "start";
constexpr const char* end_message = "end";
constexpr const char* close_connection_message = "close";

struct ImageInfo
{
	unsigned width;
	unsigned height;
	short depth;
};

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

	// Main loop
	std::vector<char> frame;
	while (!glfwWindowShouldClose(window))
	{
		float ratio;
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		ratio = width / (float)height;
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
		glfwSwapBuffers(window);
		glfwPollEvents();

		frame.resize(width * height * 3);
		glReadBuffer(GL_FRONT);
		glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, (void*)frame.data());
		send_to_udp(sock, address, start_message, strlen(start_message));
		ImageInfo info;
		info.width = width;
		info.height = height;
		info.depth = 3;
		send_to_udp(sock, address, (char*)&info, sizeof(ImageInfo));
		if (send_to_udp(sock, address, frame.data(), frame.size()) == SOCKET_ERROR)
		{
			std::cout << "send_to_udp() failed with error code : " << WSAGetLastError() << std::endl;
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		send_to_udp(sock, address, end_message, strlen(end_message));
	}
	send_to_udp(sock, address, end_message, strlen(end_message));
	send_to_udp(sock, address, close_connection_message, strlen(close_connection_message));

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}