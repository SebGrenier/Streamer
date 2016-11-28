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

class Decoder {
public:
    Decoder(CODEC codec);
    ~Decoder();

    void decode(unsigned char *data, unsigned length);
    bool has_frame();
    uintptr_t frame();

private:
    void parse_nal(unsigned char *data, unsigned length);
    void nal_hit(unsigned char *data = nullptr, unsigned length = 0, unsigned offset = 0);
    std::vector<unsigned char> _buffer;

    AVCodec *_codec;
    AVCodecContext *_codec_context;
    int _video_stream_index;
};
