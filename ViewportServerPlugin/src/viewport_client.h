#pragma once
#include "common.h"
#include "streamer.h"
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <engine_plugin_api/plugin_api.h>
#include <plugin_foundation/id_string.h>

#include <thread>

#include "nvenc/nv_encode_session.h"


using server = websocketpp::server<websocketpp::config::asio>;
using msg_ptr = server::message_ptr;

class ViewportClient
{
public:
	explicit ViewportClient(const EnginePluginApis &apis, const CommunicationHandlers &comm, websocketpp::connection_hdl hdl, AllocatorObject *allocator);
	~ViewportClient();

	void set_id(int id) { _id = id; }

	bool closed() const { return _closed; }
	bool stream_opened() const { return _stream_opened; }

	void close();

	void open_stream(void *win, stingray_plugin_foundation::IdString32 buffer_name);
	void close_stream();
	void resize_stream();

	// Socket handlers
	void handle_message(websocketpp::connection_hdl hdl, msg_ptr msg);
	void handle_close(websocketpp::connection_hdl hdl);
	void handle_fail(websocketpp::connection_hdl hdl);

	void run();
	void stop();

	void render(unsigned sch);

private:
	void info(const std::string &message);
	void warning(const std::string &message);
	void error(const std::string &message);
	void send_text(const std::string &message);
	void send_binary(void *buffer, int size);

	bool window_valid() const;

	// Connection info
	websocketpp::connection_hdl _socket_handle;
	bool _closed;
	int _id;

	// Stream info
	bool _stream_opened;
	void *_win;
	stingray_plugin_foundation::IdString32 _buffer_name;

	// Engine apis
	AllocatorObject *_allocator;
	AllocatorApi *_alloc_api;
	StreamCaptureApi *_sc_api;
	RenderInterfaceApi *_ri_api;
	RenderBufferApi *_rb_api;
	ScriptApi *_c_api;
	ProfilerApi *_prof_api;

	// Threading
	bool _quit;
	std::thread *_thread_id;
	std::mutex _stream_mutex;

	// Communication handlers
	CommunicationHandlers _comm;

	// Stream/Compression engine
	Streamer *_streamer;
	StreamOptions _stream_options;

	NVEncodeSession *_nv_encode_session;
};
