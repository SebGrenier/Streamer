#include "streamer.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <iostream>

int round_to_higher_multiple_of_two(int value)
{
	return (value & 0x01) ? value + 1 : value;
}

Streamer::Streamer()
	: _scale_context(nullptr)
	, _codec(nullptr)
	, _codec_context(nullptr)
	, _port(0)
	, _initialized(false)
	, _stream_opened(false)
{}

Streamer::~Streamer()
{
	if (_stream_opened) {
		close_stream();
	}
	if (_initialized) {
		shutdown();
	}
}


bool Streamer::init()
{
	std::cout << "Registering codecs" << std::endl;
	avcodec_register_all();
	std::cout << "Done." << std::endl;

	_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!_codec) {
		std::cout << "Codec not found" << std::endl;
		return false;
	}
	std::cout << "H264 codec found" << std::endl;

	_codec_context = avcodec_alloc_context3(_codec);

	_codec_context->bit_rate = 400000;
	_codec_context->time_base.num = 1;                                   // framerate numerator
	_codec_context->time_base.den = 60;                                  // framerate denominator
	_codec_context->gop_size = 10;                                       // emit one intra frame every ten frames
	_codec_context->max_b_frames = 1;                                    // maximum number of b-frames between non b-frames
	_codec_context->keyint_min = 1;                                      // minimum GOP size
	_codec_context->i_quant_factor = (float)0.71;                        // qscale factor between P and I frames
	//_codec_context->b_frame_strategy = 20;                               ///// find out exactly what this does
	_codec_context->qcompress = (float)0.6;                              ///// find out exactly what this does
	_codec_context->qmin = 20;                                           // minimum quantizer
	_codec_context->qmax = 51;                                           // maximum quantizer
	_codec_context->max_qdiff = 4;                                       // maximum quantizer difference between frames
	_codec_context->refs = 4;                                            // number of reference frames
	_codec_context->trellis = 1;                                         // trellis RD Quantization
	_codec_context->pix_fmt = AV_PIX_FMT_RGB24;                           // universal pixel format for video encoding
	_codec_context->codec_id = AV_CODEC_ID_H264;
	_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;

	_initialized = true;
	return true;
}

void Streamer::shutdown()
{
	avcodec_free_context(&_codec_context);
	_initialized = false;
}


bool Streamer::open_stream(int width, int height)
{
	auto new_width = round_to_higher_multiple_of_two(width);
	auto new_height = round_to_higher_multiple_of_two(height);
	_codec_context->width = new_width;  // resolution must be a multiple of two (1280x720),(1900x1080),(720x480)
	_codec_context->height = new_height;
	
	if (avcodec_open2(_codec_context, _codec, nullptr) < 0) {
		std::cout << "Could not open codec" << std::endl; // opening the codec
		return false;
	}
	std::cout << "H264 codec opened" << std::endl;

	_stream_opened = true;
	return true;
}

void Streamer::close_stream()
{
	avcodec_close(_codec_context);
	_stream_opened = false;
}
