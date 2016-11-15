#pragma once
#include <atomic>
#include <mutex>
#include <vector>

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

struct StreamConfig
{
	std::function<void(uint8_t*, int)> on_packet_write;
	std::function<void(const std::string&)> info;
	std::function<void(const std::string&)> warning;
	std::function<void(const std::string&)> error;
};

class Streamer
{
public:
	explicit Streamer(StreamConfig config);
	~Streamer();

	bool init();
	void shutdown();

	bool open_stream(int width, int height, short depth, const std::string &format, const std::string &path);
	void close_stream();

	void stream_frame(const uint8_t *frame, int width, int height, short depth);

	bool initialized() const { return _initialized; }
	bool stream_opened() const { return _stream_opened; }

	const StreamingInfo& streaming_info() const { return _streaming_info; }
private:
	bool initialize_codec_context(AVCodecContext *codec_context, AVStream *stream, int width, int height);
	int encode_frame(AVFrame *frame, AVCodecContext *context);

	int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);

	SwsContext *_scale_context;
	AVCodec *_codec;
	AVFormatContext *_format_context;
	AVStream *_video_stream;

	StreamingInfo _streaming_info;
	bool _initialized;
	bool _stream_opened;
	int64_t _frame_counter;

	StreamConfig _config;
};