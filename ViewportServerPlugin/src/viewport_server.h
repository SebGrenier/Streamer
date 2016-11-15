#pragma once
#include "common.h"
#include "viewport_client.h"

#include <vector>
#include <mutex>

class ViewportServer
{
public:
	ViewportServer();
	~ViewportServer();

	void init(EnginePluginApis apis, const char *ip, int port);
	void uninit();
private:
	void start_ws_server(const char *ip, int port);
	void stop_ws_server();

	void run_client(ViewportClient *client);
	void close_all_clients();

	void info(const std::string &message);
	void warning(const std::string &message);
	void error(const std::string &message);
	void info(const char *message);
	void warning(const char *message);
	void error(const char *message);

	bool _initialized;
	EnginePluginApis _apis;
	AllocatorObject *_allocator;

	std::thread *_ws_thread;
	std::mutex _client_mutex;
	std::vector<ViewportClient*> _clients;
	bool _quit;
};
