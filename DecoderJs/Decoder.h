//
// Created by sebastien on 11/28/16.
//
#pragma once

#include <stdint.h>
#include <vector>
#include <emscripten/val.h>
#include <cstddef>

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

    void decode(uintptr_t data, size_t length);
    bool get_hasFrame() const { return _has_frame; }
    emscripten::val get_frame() { return emscripten::val(emscripten::typed_memory_view(_decoded_frame_size, _decoded_frame)); }

    int get_width() const;
    int get_height() const;

private:
    void parse_nal(uint8_t *data, size_t length);
    void nal_hit(uint8_t *data = nullptr, size_t length = 0);
    void frame_decode(uint8_t *data, size_t length);
    void open_scaling_context(size_t width, size_t height);
    void close_scaling_context();
    std::vector<std::vector<uint8_t>* > _buffers;

    AVCodec *_codec;
    AVCodecContext *_codec_context;
    AVFrame *_frame;
    SwsContext *_scaling_context;

    bool _has_frame;
    bool _scaling_context_opened;

    uint8_t *_decoded_frame;
    size_t _decoded_frame_size;
};
