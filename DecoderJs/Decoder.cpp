//
// Created by sebastien on 11/28/16.
//
#include "Decoder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libavutil/error.h>
}

AVCodecID get_codec_id(CODEC codec) {
    switch (codec) {
        case H264:
            return AV_CODEC_ID_H264;
        default:
            return AV_CODEC_ID_H264;
    }
}

Decoder::Decoder(CODEC codec)
	: _scaling_context(nullptr)
	, _has_frame(false)
	, _scaling_context_opened(false)
{
    av_register_all();
    avcodec_register_all();
    _codec = avcodec_find_decoder(get_codec_id(codec));
    _codec_context = avcodec_alloc_context3(_codec);
	avcodec_open2(_codec_context, _codec, nullptr);

	_frame = av_frame_alloc();
}

Decoder::~Decoder()
{
    avcodec_close(_codec_context);
}

void Decoder::decode(unsigned char *data, unsigned length)
{
	parse_nal(data, length);
}

void Decoder::parse_nal(unsigned char *data, unsigned length)
{
    if (length == 0){
        return;
    }

    unsigned b = 0;
    unsigned lastStart = 0;

    unsigned zeroCnt = 0;

    for (b = 0; b < length; ++b){
        if (data[b] == 0){
            ++zeroCnt;
        }else{
            if (data[b] == 1){
                if (zeroCnt >= 3){
                    if (lastStart < b - 3){
                        nal_hit(&data[lastStart], (b-3) - lastStart);
                        lastStart = b - 3;
                    }else if (length > 0){
                        nal_hit();
                    }
                }
            }
            zeroCnt = 0;
        }
    }
    if (lastStart < length){
		std::vector<unsigned char> *new_data = new std::vector<unsigned char>(&data[lastStart], &data[length - 1]);
        _buffers.push_back(new_data);
    }
}

void Decoder::nal_hit(unsigned char *data, unsigned length)
{
    if (data != nullptr){
		std::vector<unsigned char> *new_data = new std::vector<unsigned char>(data, data + length);
        _buffers.push_back(new_data);
    };

	const int nb_buffer = _buffers.size();
	if (nb_buffer > 0) {
		unsigned total_size = 0;
		for (int i = 0; i < nb_buffer; ++i) {
			total_size += _buffers[i]->size();
		}

		unsigned char *frame_buffer = new unsigned char[total_size];
		unsigned index = 0;
		for (int i = 0; i < nb_buffer; ++i) {
			memcpy(&frame_buffer[index], _buffers[i]->data(), _buffers[i]->size());
			index += _buffers[i]->size();
			delete _buffers[i];
		}
		frame_decode(frame_buffer, total_size);

		_buffers.clear();
	}
}

void Decoder::frame_decode(unsigned char* data, unsigned length)
{
	AVPacket        packet;
	av_init_packet(&packet);

	packet.data = data;
	packet.size = (int)length;
	int success = avcodec_send_packet(_codec_context, &packet);

	success = avcodec_receive_frame(_codec_context, _frame);

	if (success == 0) {
		if (!_scaling_context_opened) {
			open_scaling_context(_frame->width, _frame->height);
		}
		_has_frame = true;
	} else {
		if (success == AVERROR_EOF) {
			close_scaling_context();
		}

		_has_frame = false;
	}

	return;
}

void Decoder::open_scaling_context(unsigned width, unsigned height)
{
	_scaling_context = sws_getContext(
		width, // src width
		height, // src height
		AV_PIX_FMT_YUV420P, // src format
		width, // dest width
		height, // dest height
		AV_PIX_FMT_RGBA, // dest format
		SWS_FAST_BILINEAR, // scaling flag
		nullptr, // src filter
		nullptr, // dest filter
		nullptr // params
	);
	_scaling_context_opened = true;
}

void Decoder::close_scaling_context()
{
	_scaling_context_opened = false;
}

