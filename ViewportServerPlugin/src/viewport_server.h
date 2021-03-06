#pragma once
#include "common.h"
#include "viewport_client.h"
#include "function_stream.h"

#include <vector>
#include <mutex>

class ViewportServer
{
public:
	ViewportServer();
	~ViewportServer();

	void init(EnginePluginApis apis);
	void uninit();
	void update();
	void render(unsigned sch);

	void open_connection(const char *ip, int port);
	void close_connection();

	bool initialized() const { return _initialized; }
	bool started() const { return _server_started; }

	void info(const std::string &message);
	void warning(const std::string &message);
	void error(const std::string &message);

	EnginePluginApis& apis() { return _apis; }
	AllocatorObject* allocator() { return _allocator; }
private:
	void start_ws_server(const char *ip, int port);
	void stop_ws_server();

	void run_client(ViewportClient *client);
	void run_all_clients();
	void sweep_clients();
	void close_all_clients();


	void info(const char *message);
	void warning(const char *message);
	void error(const char *message);

	bool _initialized;
	bool _server_started;
	EnginePluginApis _apis;
	AllocatorObject *_allocator;

	std::thread *_ws_thread;
	std::mutex _client_mutex;
	std::vector<ViewportClient*> _clients;
	bool _quit;

	// WSPPLogger
	ofunctionstream *_ws_ostream;
};
