#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <websocketpp/transport/base/connection.hpp>

struct AVFrame;
struct SwsContext;
struct AVCodec;
struct AVCodecContext;
struct AVFormatContext;
struct AVStream;
struct AVPacket;
struct AVRational;

struct StreamingInfo
{
	int width;
	int height;
	short depth;
};

struct FrameInfo
{
	StreamingInfo info;
	std::vector<uint8_t> data;
};

bool operator != (const StreamingInfo &lhs, const StreamingInfo rhs);

class Streamer
{
public:
	Streamer();
	~Streamer();

	bool init();
	void shutdown();

	bool open_stream(int width, int height, short depth, const std::string &format, const std::string &path);
	void close_stream();

	void stream_frame(const uint8_t *frame, int width, int height, short depth);

	bool initialized() const { return _initialized; }
	bool stream_opened() const { return _stream_opened; }

	const StreamingInfo& streaming_info() const { return _streaming_info; }

	void send_frame_ws(AVPacket *pkt);
	void send_packet_buffer(void* buffer, int size);
private:
	bool initialize_codec_context(AVCodecContext *codec_context, int width, int height) const;
	int encode_frame(AVFrame *frame, AVCodecContext *context);
	void run_websocket_thread();

	int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);

	SwsContext *_scale_context;
	AVCodec *_codec;
	AVFormatContext *_format_context;
	AVStream *_video_stream;
	AVCodecContext *_codec_context;

	StreamingInfo _streaming_info;
	bool _initialized;
	bool _stream_opened;
	int64_t _frame_counter;

	std::thread *_ws_thread;
	std::mutex _connection_mutex;
	std::vector<websocketpp::connection_hdl> _connections;
};