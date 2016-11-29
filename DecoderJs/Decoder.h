//
// Created by sebastien on 11/28/16.
//
#pragma once

#include <stdint.h>
#include <vector>
#include <emscripten/val.h>

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
    bool get_hasFrame() const { return _has_frame; }
    emscripten::val get_frame() { return emscripten::val(emscripten::typed_memory_view(_decoded_frame_size, _decoded_frame)); }

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

    unsigned char *_decoded_frame;
    unsigned _decoded_frame_size;
};
