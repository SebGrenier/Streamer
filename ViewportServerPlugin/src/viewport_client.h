#pragma once
#include "common.h"
#include "streamer.h"
#include <plugin_foundation/id_string.h>

#include <thread>

class ViewportServer;

enum class CaptureMode
{
	CAPTURE_TYPE_UNKNOWN = 0,
	STREAMED_UNCOMPRESSED = 1,
	STREAMED_COMPRESSED = 2,
	STREAMED_COMPRESSED_LZ4 = 3,
	STREAMED_COMPRESSED_H264 = 4
};

class ViewportClient
{
public:
	explicit ViewportClient(ViewportServer *server, CommunicationHandlers comm, websocketpp::connection_hdl hdl, AllocatorObject *allocator);
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

	ViewportServer *_server;

	// Connection info
	websocketpp::connection_hdl _socket_handle;
	bool _closed;
	int _id;

	// Stream info
	bool _stream_opened;
	void *_win;
	stingray_plugin_foundation::IdString32 _buffer_name;
	CaptureMode _mode;

	AllocatorObject *_allocator;

	// Threading
	bool _quit;
	std::thread *_thread_id;
	std::mutex _stream_mutex;

	// Communication handlers
	CommunicationHandlers _comm;

	// Stream/Compression engine
	Streamer *_streamer;
	EncodingOptions _stream_options;
};
