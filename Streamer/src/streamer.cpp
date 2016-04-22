#include "streamer.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <iostream>

using critical_section_holder = std::lock_guard<std::mutex>;

constexpr long long frame_rate = (long long)(1.0f / 60.0f * 1000.0f);

int round_to_higher_multiple_of_two(int value)
{
	return (value & 0x01) ? value + 1 : value;
}

bool operator != (const StreamingInfo &lhs, const StreamingInfo rhs)
{
	return lhs.width != rhs.width ||
		lhs.height != rhs.height;
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	/*printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
	av_ts_make_string(pkt->pts), av_ts_make_string(pkt->pts, time_base),
	av_ts_make_string(pkt->dts), av_ts_make_string(pkt->dts, time_base),
	av_ts_make_string(pkt->duration), av_ts_make_string(pkt->duration, time_base),
	pkt->stream_index);*/
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	pkt->pts = av_rescale_q_rnd(pkt->pts, *time_base, st->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->dts = av_rescale_q_rnd(pkt->dts, *time_base, st->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	pkt->duration = av_rescale_q(pkt->duration, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	log_packet(fmt_ctx, pkt);
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

Streamer::Streamer()
	: _scale_context(nullptr)
	, _codec(nullptr)
	, _format_context(nullptr)
	, _video_stream(nullptr)
	, _initialized(false)
	, _stream_opened(false)
	, _frame_counter(0)
	, _quit_thread(false)
	, _streamer_thread(nullptr)
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
	std::cout << "Registering formats" << std::endl;
	av_register_all();
	std::cout << "Registering codecs" << std::endl;
	avcodec_register_all();
	std::cout << "Initializing network components" << std::endl;
	avformat_network_init();
	std::cout << "Done." << std::endl;

	_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!_codec) {
		std::cout << "Codec not found" << std::endl;
		return false;
	}
	std::cout << "H264 codec found" << std::endl;

	_initialized = true;
	return true;
}

void Streamer::shutdown()
{
	avformat_network_deinit();
	_initialized = false;
}


bool Streamer::open_stream(int width, int height, short depth, const std::string &format, const std::string &path)
{
	_frame_counter = 0;

	auto new_width = round_to_higher_multiple_of_two(width);
	auto new_height = round_to_higher_multiple_of_two(height);

	avformat_alloc_output_context2(&_format_context, nullptr, format.c_str(), path.c_str());
	if (_format_context == nullptr) {
		std::cout << "Failed to allocate format context for " << path << " with format " << format << std::endl;
		return false;
	}

	_video_stream = avformat_new_stream(_format_context, _codec);
	if (_video_stream == nullptr) {
		std::cout << "Failed to open video stream for " << path << " with format " << format << std::endl;
		return false;
	}

	auto *codec_context = _video_stream->codec;
	if (!initialize_codec_context(codec_context, _video_stream, new_width, new_height)) {
		std::cout << "Could not initialize codec context" << std::endl;
		return false;
	}

	if ((_format_context->oformat->flags & AVFMT_NOFILE) == 0) {
		auto ret = avio_open(&_format_context->pb, path.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
			char error_buff[80];
			av_make_error_string(error_buff, 80, ret);
			std::cout << "Could not open output " << path << " : " << error_buff;
			return false;
		}
	}

	if (avformat_write_header(_format_context, nullptr) < 0) {
		std::cout << "Could not write header" << std::endl;
		return false;
	}

	_scale_context = sws_getContext(
		width, // src width
		height, // src height
		depth == 3 ? AV_PIX_FMT_RGB24 : AV_PIX_FMT_RGBA, // src format
		codec_context->width, // dest width
		codec_context->height, // dest height
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
	_streaming_info.depth = depth;
	_current_frame.info = _streaming_info;
	_current_frame.data.resize(width * height * depth, 0);

	_quit_thread = false;

	_streamer_thread = new std::thread(&Streamer::run_thread, this);

	_stream_opened = true;
	return true;
}

void Streamer::close_stream()
{
	_quit_thread = true;
	_streamer_thread->join();
	delete _streamer_thread;

	avformat_free_context(_format_context);
	avcodec_close(_video_stream->codec);
	_stream_opened = false;

	if (_scale_context != nullptr) {
		sws_freeContext(_scale_context);
		_scale_context = nullptr;
	}
}

void Streamer::stream_frame(const uint8_t* frame, int width, int height, short depth)
{
	{
		critical_section_holder holder(_frame_mutex);
		std::copy(frame, frame + width * height * depth, _current_frame.data.begin());
	}
}

bool Streamer::initialize_codec_context(AVCodecContext* codec_context, AVStream *stream, int width, int height) const
{
	codec_context->width = width;  // resolution must be a multiple of two (1280x720),(1900x1080),(720x480)
	codec_context->height = height;

	codec_context->bit_rate = 400000;
	stream->time_base.num = 1;                                   // framerate numerator
	stream->time_base.den = 60;                                  // framerate denominator
	codec_context->gop_size = 10;                                       // emit one intra frame every ten frames
	codec_context->max_b_frames = 1;                                    // maximum number of b-frames between non b-frames
	codec_context->keyint_min = 1;                                      // minimum GOP size
	codec_context->i_quant_factor = (float)0.71;                        // qscale factor between P and I frames
	//codec_context->b_frame_strategy = 20;                               ///// find out exactly what this does
	codec_context->qcompress = (float)0.6;                              ///// find out exactly what this does
	codec_context->qmin = 20;                                           // minimum quantizer
	codec_context->qmax = 51;                                           // maximum quantizer
	codec_context->max_qdiff = 4;                                       // maximum quantizer difference between frames
	codec_context->refs = 4;                                            // number of reference frames
	codec_context->trellis = 1;                                         // trellis RD Quantization
	codec_context->pix_fmt = AV_PIX_FMT_YUV420P;                           // universal pixel format for video encoding
	codec_context->codec_id = AV_CODEC_ID_H264;
	codec_context->codec_type = AVMEDIA_TYPE_VIDEO;

	if (codec_context->codec_id == AV_CODEC_ID_H264) {
		av_opt_set(codec_context->priv_data, "preset", "ultrafast", 0);
		av_opt_set(codec_context->priv_data, "tune", "zerolatency", 0);
	}

	/* Some formats want stream headers to be separate. */
	if (_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
		codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	if (avcodec_open2(codec_context, _codec, nullptr) < 0) {
		std::cout << "Could not open codec" << std::endl; // opening the codec
		return false;
	}
	std::cout << "H264 codec opened" << std::endl;

	return true;
}

int Streamer::encode_frame(AVFrame *frame, AVCodecContext *context) const
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
		success = write_frame(_format_context, &_video_stream->time_base, _video_stream, &packet);
		if (success < 0) {
			std::cout << "Error streaming frame" << std::endl;
		}
		//av_packet_unref(&packet);
	}

	return got_packet;
}

void Streamer::run_thread()
{
	while(!_quit_thread) {
		AVFrame* inpic = av_frame_alloc(); // mandatory frame allocation

		{
			std::lock_guard<std::mutex> critical_section(_frame_mutex);

			inpic->format = AV_PIX_FMT_RGB24;
			inpic->width = _video_stream->codec->width;
			inpic->height = _video_stream->codec->height;
			auto success = av_image_fill_arrays(inpic->data, inpic->linesize, _current_frame.data.data(), AV_PIX_FMT_RGB24, _current_frame.info.width, _current_frame.info.height, 32);
			if (success < 0) {
				std::cout << "Error transforming data into frame" << std::endl;
				av_frame_free(&inpic);
				return;
			}
		}

		AVFrame* outpic = av_frame_alloc();
		outpic->format = AV_PIX_FMT_YUV420P;
		outpic->width = _video_stream->codec->width;
		outpic->height = _video_stream->codec->height;
		//outpic->pts = (int64_t)((float)i * (1000.0 / ((float)(_codec_context->time_base.den))) * 90);                              // setting frame pts
		//outpic->pts = av_frame_get_best_effort_timestamp(outpic);
		outpic->pts = _frame_counter++;
		av_image_alloc(outpic->data, outpic->linesize, _video_stream->codec->width, _video_stream->codec->height, _video_stream->codec->pix_fmt, 32);

		sws_scale(_scale_context, inpic->data, inpic->linesize, 0, _video_stream->codec->height, outpic->data, outpic->linesize);          // converting frame size and format

		encode_frame(outpic, _video_stream->codec);

		av_freep(&outpic->data[0]);
		av_frame_free(&inpic);
		av_frame_free(&outpic);

		std::this_thread::sleep_for(std::chrono::milliseconds(frame_rate));
	}

	/* get the delayed frames */
	int got_packet;
	do {
		got_packet = encode_frame(nullptr, _video_stream->codec);
	} while (got_packet);

	av_write_trailer(_format_context);

	if (!(_format_context->flags & AVFMT_NOFILE)) {
		/* Close the output. */
		avio_closep(&_format_context->pb);
	}

}

