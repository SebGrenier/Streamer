#include "streamer.h"
#include "strategies/IStreamingStrategy.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <iostream>

int round_to_higher_multiple_of_two(int value)
{
	return (value & 0x01) ? value + 1 : value;
}

bool operator != (const StreamingInfo &lhs, const StreamingInfo rhs)
{
	return lhs.width != rhs.width ||
		lhs.height != rhs.height;
}

int encode_frame(AVFrame *frame, AVCodecContext *context, IStreamingStrategy *strategy)
{
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = nullptr;
	packet.size = 0;
	auto got_packet = 0;
	auto success = avcodec_encode_video2(context, &packet, frame, &got_packet);
	if (success < 0) {
		std::cout << "Error encoding frame" << std::endl;
	}

	if (got_packet != 0) {
		success = strategy->push_frame(packet.data, packet.size);
		if (!success) {
			std::cout << "Error streaming frame" << std::endl;
		}
		av_packet_unref(&packet);
	}

	return got_packet;
}

Streamer::Streamer()
	: _scale_context(nullptr)
	, _codec(nullptr)
	, _codec_context(nullptr)
	, _initialized(false)
	, _stream_opened(false)
	, _streaming_strategy(nullptr)
	, _frame_counter(0)
{}

Streamer::~Streamer()
{
	if (_stream_opened) {
		close_stream();
	}
	if (_initialized) {
		shutdown();
	}

	_streaming_strategy = nullptr;
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
	_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;                           // universal pixel format for video encoding
	_codec_context->codec_id = AV_CODEC_ID_H264;
	_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;

	if (_codec_context->codec_id == AV_CODEC_ID_H264) {
		av_opt_set(_codec_context->priv_data, "preset", "ultrafast", 0);
		av_opt_set(_codec_context->priv_data, "tune", "zerolatency", 0);
	}

	_initialized = true;
	return true;
}

void Streamer::shutdown()
{
	avcodec_free_context(&_codec_context);
	_initialized = false;
}


bool Streamer::open_stream(int width, int height, IStreamingStrategy *strategy)
{
	_frame_counter = 0;

	auto new_width = round_to_higher_multiple_of_two(width);
	auto new_height = round_to_higher_multiple_of_two(height);
	_codec_context->width = new_width;  // resolution must be a multiple of two (1280x720),(1900x1080),(720x480)
	_codec_context->height = new_height;

	if (avcodec_open2(_codec_context, _codec, nullptr) < 0) {
		std::cout << "Could not open codec" << std::endl; // opening the codec
		return false;
	}
	std::cout << "H264 codec opened" << std::endl;

	_streaming_strategy = strategy;
	if (_streaming_strategy != nullptr && !_streaming_strategy->open_stream()) {
		std::cout << "Failed to open stream" << std::endl;
		return false;
	}

	_scale_context = sws_getContext(
		_codec_context->width, // src width
		_codec_context->height, // src height
		AV_PIX_FMT_RGB24, // src format
		_codec_context->width, // dest width
		_codec_context->height, // dest height
		AV_PIX_FMT_YUV420P, // dest format
		SWS_FAST_BILINEAR, // scaling flag
		nullptr, // src filter
		nullptr, // dest filter
		nullptr // params
		);
	if (_scale_context == nullptr) {
		std::cout << "Failed to allocate scale context" << std::endl;
		return false;
	}

	_streaming_info.height = height;
	_streaming_info.width = width;
	_stream_opened = true;
	return true;
}

void Streamer::close_stream()
{
	/* get the delayed frames */
	auto got_packet = 0;
	do {
		got_packet = encode_frame(nullptr, _codec_context, _streaming_strategy);
	} while (got_packet);

	avcodec_close(_codec_context);
	_stream_opened = false;

	if (_streaming_strategy != nullptr) {
		_streaming_strategy->close_stream();
		_streaming_strategy = nullptr;
	}

	if (_scale_context != nullptr) {
		sws_freeContext(_scale_context);
		_scale_context = nullptr;
	}
}

void Streamer::stream_frame(const uint8_t* frame, const int frame_size)
{
	AVFrame* inpic = av_frame_alloc(); // mandatory frame allocation
	inpic->format = AV_PIX_FMT_RGB24;
	inpic->width = _codec_context->width;
	inpic->height = _codec_context->height;
	auto success = av_image_fill_arrays(inpic->data, inpic->linesize, frame, AV_PIX_FMT_RGB24, _codec_context->width, _codec_context->height, 32);
	if (success < 0) {
		std::cout << "Error transforming data into frame" << std::endl;
		av_frame_free(&inpic);
		return;
	}

	AVFrame* outpic = av_frame_alloc();
	outpic->format = AV_PIX_FMT_YUV420P;
	outpic->width = _codec_context->width;
	outpic->height = _codec_context->height;
	//outpic->pts = (int64_t)((float)i * (1000.0 / ((float)(_codec_context->time_base.den))) * 90);                              // setting frame pts
	//outpic->pts = av_frame_get_best_effort_timestamp(outpic);
	outpic->pts = _frame_counter++;
	av_image_alloc(outpic->data, outpic->linesize, _codec_context->width, _codec_context->height, _codec_context->pix_fmt, 32);

	sws_scale(_scale_context, inpic->data, inpic->linesize, 0, _codec_context->height, outpic->data, outpic->linesize);          // converting frame size and format

	encode_frame(outpic, _codec_context, _streaming_strategy);

	av_freep(&outpic->data[0]);
	av_frame_free(&inpic);
	av_frame_free(&outpic);
}

