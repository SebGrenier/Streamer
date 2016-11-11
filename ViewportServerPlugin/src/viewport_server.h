#pragma once
#include <websocketpp/transport/base/connection.hpp>
#include <vector>
#include <mutex>

struct LoggingApi;
struct StreamCaptureApi;
struct RenderBufferApi;
struct ThreadApi;
struct AllocatorApi;
struct AllocatorObject;

struct EnginePluginApis
{
	LoggingApi *logging_api;
	StreamCaptureApi *stream_capture_api;
	RenderBufferApi *render_buffer_api;
	ThreadApi *thread_api;
	AllocatorApi *allocator_api;
};

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

	void run_client();
	void close_all_clients();

	void info(const char *message);
	void warning(const char *message);
	void error(const char *message);

	bool _initialized;
	EnginePluginApis _apis;
	AllocatorObject *_allocator;

	std::thread *_ws_thread;
	std::mutex _client_mutex;
	std::vector<std::thread*> _clients;
	bool _quit;
};
