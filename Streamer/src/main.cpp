#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>

constexpr int max_buffer_size = 63 * 1024;
constexpr const char* server_address = "127.0.0.1";
constexpr short server_port = 12345;

int count = 0;

enum class RECEIVING_STATE {LISTENING, BUFFERING_IMAGE};

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

void save_image(const std::vector<char> image, const ImageInfo &image_info, const std::string &filename)
{
	std::ofstream file;
	file.open(filename);

	if (file.is_open()) {
		file.write(image.data(), image.size());
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
	std::vector<char> image;
	image.resize(buffer_size);
	while(total_received < buffer_size) {
		recv_len = recvfrom(s, buffer, max_buffer_size, 0, (sockaddr *)&address, &address_size);
		if (recv_len == SOCKET_ERROR) {
			std::cout << "recvfrom() failed with error code : " << WSAGetLastError() << std::endl;
			return;
		}

		if (_strcmpi(buffer, "end") == 0) {
			std::cout << "Error: received end message before end of buffer" << std::endl;
			return;
		}

		image.insert(image.end(), buffer, buffer + recv_len);
		total_received += recv_len;
	}

	// Image is complete. Save it.
	std::stringstream ss;
	ss << "image_";
	ss << count++;
	ss << ".data";
	save_image(image, info, ss.str());
}

int main(int argc, char **argv)
{
	SOCKET sock;
	sockaddr_in si_server, si_client;
	int slen, recv_len;
	char buf[max_buffer_size];
	WSADATA wsa;

	slen = sizeof(si_client);

	// Initialise winsock
	std::cout << "Initialising Winsock..." << std::endl;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		std::cout << "Failed. Error Code : " << WSAGetLastError();
		exit(EXIT_FAILURE);
	}
	std::cout << "Initialised." << std::endl;

	//Create a socket
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		std::cout << "Could not create socket : " << WSAGetLastError();
	}
	printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	si_server.sin_family = AF_INET;
	si_server.sin_port = htons(server_port);
	inet_pton(AF_INET, server_address, &si_server.sin_addr);

	//Bind
	if (bind(sock, (sockaddr *)&si_server, sizeof(si_server)) == SOCKET_ERROR)
	{
		std::cout << "Bind failed with error code : " << WSAGetLastError();
		exit(EXIT_FAILURE);
	}
	std::cout << "Bind done" << std::endl;

	//keep listening for data
	auto listening_state = RECEIVING_STATE::LISTENING;
	while (1)
	{
		std::cout << "Waiting for data..." << std::endl;
		std::cout.flush();

		//clear the buffer by filling null, it might have previously received data
		memset(buf, '\0', max_buffer_size);

		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(sock, buf, max_buffer_size, 0, (sockaddr *)&si_client, &slen)) == SOCKET_ERROR)
		{
			std:: cout << "recvfrom() failed with error code : " << WSAGetLastError() << std::endl;
			break;
		}

		//print details of the client/peer and the data received
		char address[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &si_client.sin_addr, address, INET_ADDRSTRLEN);
		std::cout << "Received packet from " << address << ":" << ntohs(si_client.sin_port) << std::endl;
		//std::cout << "Data: " << buf << std::endl;

		// Break out of loop if we receive a close command
		if (_strcmpi(buf, "close") == 0) {
			break;
		}
		
		if (_strcmpi(buf, "start") == 0 && listening_state == RECEIVING_STATE::LISTENING) {
			receive_image(sock, si_client);
		}
	}

	closesocket(sock);
	WSACleanup();

	return 0;
}