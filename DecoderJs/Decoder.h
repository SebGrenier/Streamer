//
// Created by sebastien on 11/28/16.
//
#pragma once

#include <stdint.h>
#include <vector>

struct AVFrame;
struct SwsContext;
struct AVCodec;
struct AVCodecContext;
struct AVFormatContext;
struct AVStream;

enum CODEC {
    H264 = 0
};

class Decoder
{
public:
    Decoder(CODEC codec);
    ~Decoder();

    void decode(unsigned char *data, unsigned length);
	bool has_frame() const { return _has_frame; }
    uintptr_t frame();

private:
    void parse_nal(unsigned char *data, unsigned length);
    void nal_hit(unsigned char *data = nullptr, unsigned length = 0);
	void frame_decode(unsigned char *data, unsigned length);
	void open_scaling_context(unsigned width, unsigned height);
	void close_scaling_context();
    std::vector<std::vector<unsigned char>* > _buffers;

    AVCodec *_codec;
    AVCodecContext *_codec_context;
	AVFrame *_frame;
	SwsContext *_scaling_context;

	bool _has_frame;
	bool _scaling_context_opened;
};
