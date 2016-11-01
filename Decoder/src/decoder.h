#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

struct AVFrame;
struct SwsContext;
struct AVCodec;
struct AVCodecContext;
struct AVFormatContext;
struct AVStream;

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

class Decoder
{
public:
	Decoder();
	~Decoder();

	bool init();
	void shutdown();

	bool open_stream(const std::string &format, const std::string &path);
	void close_stream();

	const FrameInfo& get_current_frame() const { return _current_frame; }

	bool initialized() const { return _initialized; }
	bool stream_opened() const { return _stream_opened; }

	const StreamingInfo& streaming_info() const { return _streaming_info; }
private:
	void decode_frame(AVFrame *frame, AVCodecContext *context);
	void run_thread();

	SwsContext *_scale_context;
	AVCodec *_codec;
	AVFormatContext *_format_context;
	AVCodecContext *_codec_context;
	int _video_stream_index;

	StreamingInfo _streaming_info;
	bool _initialized;
	bool _stream_opened;
	int64_t _frame_counter;

	FrameInfo _current_frame;
	std::thread *_streamer_thread;
	std::mutex _frame_mutex;
	std::atomic<bool> _quit_thread;
};