#pragma once
#include <string>

class SwsContext;
class AVCodec;
class AVCodecContext;

struct StreamingInfo
{
	int width;
	int height;
};

class Streamer
{
public:
	Streamer();
	~Streamer();

	bool init();
	void shutdown();

	bool open_stream(int width, int height);
	void close_stream();


	bool initialized() const { return _initialized; }
	bool stream_opened() const { return _stream_opened; }
private:
	SwsContext *_scale_context;
	AVCodec *_codec;
	AVCodecContext *_codec_context;

	std::string _ip;
	short _port;

	StreamingInfo _streaming_info;
	bool _initialized;
	bool _stream_opened;
};