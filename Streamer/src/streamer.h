#pragma once
#include <string>

class IStreamingStrategy;
class SwsContext;
class AVCodec;
class AVCodecContext;

struct StreamingInfo
{
	int width;
	int height;
};

bool operator != (const StreamingInfo &lhs, const StreamingInfo rhs);

class Streamer
{
public:
	Streamer();
	~Streamer();

	bool init();
	void shutdown();

	bool open_stream(int width, int height, IStreamingStrategy *strategy);
	void close_stream();

	void stream_frame(const uint8_t *frame, const int frame_size);

	bool initialized() const { return _initialized; }
	bool stream_opened() const { return _stream_opened; }

	const StreamingInfo& streaming_info() const { return _streaming_info; }
private:
	SwsContext *_scale_context;
	AVCodec *_codec;
	AVCodecContext *_codec_context;

	StreamingInfo _streaming_info;
	bool _initialized;
	bool _stream_opened;
	int64_t _frame_counter;

	IStreamingStrategy *_streaming_strategy;
};