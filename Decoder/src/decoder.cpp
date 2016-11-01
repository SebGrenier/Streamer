#include "decoder.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <iostream>
#include <string>

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

int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
	int ret = avformat_match_stream_specifier(s, st, spec);
	if (ret < 0)
		std::cout << "Invalid stream specifier: " << spec << std::endl;
	return ret;
}

AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
	AVFormatContext *s, AVStream *st, AVCodec *codec)
{
	AVDictionary    *ret = nullptr;
	AVDictionaryEntry *t = nullptr;
	int flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM : AV_OPT_FLAG_DECODING_PARAM;
	char prefix = 0;
	const AVClass *cc = avcodec_get_class();

	if (!codec)
		codec = s->oformat ? avcodec_find_encoder(codec_id) : avcodec_find_decoder(codec_id);

	switch (st->codec->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		prefix = 'v';
		flags |= AV_OPT_FLAG_VIDEO_PARAM;
		break;
	case AVMEDIA_TYPE_AUDIO:
		prefix = 'a';
		flags |= AV_OPT_FLAG_AUDIO_PARAM;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		prefix = 's';
		flags |= AV_OPT_FLAG_SUBTITLE_PARAM;
		break;
	}

	while (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) {
		char *p = strchr(t->key, ':');

		/* check stream specification in opt name */
		if (p)
			switch (check_stream_specifier(s, st, p + 1)) {
			case  1: *p = 0; break;
			case  0:         continue;
			default:         return nullptr;
			}

		if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
			!codec ||
			(codec->priv_class &&
				av_opt_find(&codec->priv_class, t->key, NULL, flags,
					AV_OPT_SEARCH_FAKE_OBJ)))
			av_dict_set(&ret, t->key, t->value, 0);
		else if (t->key[0] == prefix &&
			av_opt_find(&cc, t->key + 1, NULL, flags,
				AV_OPT_SEARCH_FAKE_OBJ))
			av_dict_set(&ret, t->key + 1, t->value, 0);

		if (p)
			*p = ':';
	}
	return ret;
}

AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,
	AVDictionary *codec_opts)
{
	int i;
	AVDictionary **opts;

	if (!s->nb_streams)
		return nullptr;
	opts = (AVDictionary **)av_mallocz_array(s->nb_streams, sizeof(*opts));
	if (!opts) {
		std::cout << "Could not alloc memory for stream options." << std::endl;
		return nullptr;
	}
	for (i = 0; i < s->nb_streams; i++)
		opts[i] = filter_codec_opts(codec_opts, s->streams[i]->codec->codec_id,
			s, s->streams[i], nullptr);
	return opts;
}

Decoder::Decoder()
	: _scale_context(nullptr)
	, _codec(nullptr)
	, _format_context(nullptr)
	, _codec_context(nullptr)
	, _initialized(false)
	, _stream_opened(false)
	, _frame_counter(0)
	, _streamer_thread(nullptr)
	, _quit_thread(false)
{}

Decoder::~Decoder()
{
	if (_stream_opened) {
		close_stream();
	}
	if (_initialized) {
		shutdown();
	}
}


bool Decoder::init()
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

void Decoder::shutdown()
{
	avformat_network_deinit();
	_initialized = false;
}


bool Decoder::open_stream(const std::string &format, const std::string &path)
{
	_frame_counter = 0;

	_format_context = avformat_alloc_context();
	if (_format_context == nullptr) {
		std::cout << "Failed to allocate format context";
		return false;
	}

	_codec_context = avcodec_alloc_context3(nullptr);
	if (_codec_context == nullptr) {
		std::cout << "Failed to initialize codec context" << std::endl;
		return false;
	}

	_video_stream_index = 0;

	AVDictionary *format_opts = nullptr;
	int scan_all_pmts_set = 0;
	if (!av_dict_get(format_opts, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE)) {
		av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
		scan_all_pmts_set = 1;
	}

	auto file_iformat = av_find_input_format(format.c_str());
	if (!file_iformat) {
		std::cout << "Failed to find format" << std::endl;
		return false;
	}

	//open rtsp
	if (avformat_open_input(&_format_context, path.c_str(), file_iformat, &format_opts) != 0) {
		std::cout << "Failed to open input stream" << std::endl;
		return false;
	}

	if (scan_all_pmts_set)
		av_dict_set(&format_opts, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE);

	AVDictionary *codec_opts = nullptr;
	auto opts = setup_find_stream_info_opts(_format_context, codec_opts);
	auto orig_nb_streams = _format_context->nb_streams;

	if (avformat_find_stream_info(_format_context, opts) < 0) {
		std::cout << "Error finding stream info" << std::endl;
		return false;
	}

	for (int i = 0; i < orig_nb_streams; i++)
		av_dict_free(&opts[i]);
	av_freep(&opts);

	if (avformat_find_stream_info(_format_context, opts) < 0) {
		std::cout << "Failed to find stream" << std::endl;
		return false;
	}

	//search video stream
	for (auto i = 0; i<_format_context->nb_streams; i++) {
		if (_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			_video_stream_index = i;
	}

	//avcodec_get_context_defaults3(_codec_context, _codec);
	avcodec_copy_context(_codec_context, _format_context->streams[_video_stream_index]->codec);

	if (avcodec_open2(_codec_context, _codec, nullptr) < 0) {
		std::cout << "Failed to open codec" << std::endl;
		return false;
	}

	_scale_context = sws_getContext(_codec_context->width, _codec_context->height, _codec_context->pix_fmt, _codec_context->width, _codec_context->height,
		AV_PIX_FMT_RGB24, SWS_BICUBIC, nullptr, nullptr, nullptr);

	_quit_thread = false;

	_streamer_thread = new std::thread(&Decoder::run_thread, this);

	_stream_opened = true;
	return true;
}

void Decoder::close_stream()
{
	_quit_thread = true;
	_streamer_thread->join();
	delete _streamer_thread;

	avformat_close_input(&_format_context);
	avcodec_free_context(&_codec_context);
	avformat_free_context(_format_context);
	_stream_opened = false;

	if (_scale_context != nullptr) {
		sws_freeContext(_scale_context);
		_scale_context = nullptr;
	}
}

void Decoder::decode_frame(AVFrame *frame, AVCodecContext *context)
{
	critical_section_holder holder(_frame_mutex);
	auto size = context->width * context->height * 3;
	if (_current_frame.data.size() != size)
		_current_frame.data.resize(size);

	_current_frame.info.width = context->width;
	_current_frame.info.height = context->height;
	_current_frame.info.depth = 3;
	memcpy(&_current_frame.data[0], frame->data, size);
}

void Decoder::run_thread()
{
	AVPacket *packet = av_packet_alloc();
	av_init_packet(packet);

	AVFormatContext* oc = avformat_alloc_context();
	//oc->oformat = fmt;
	//avio_open2(&oc->pb, "test.mp4", AVIO_FLAG_WRITE,NULL,NULL);

	AVStream* stream = nullptr;
	//start reading packets from stream and write them to file
	//av_read_play(context);//play RTSP

	AVFrame* inpic = av_frame_alloc(); // mandatory frame allocation
	inpic->format = AV_PIX_FMT_YUV420P;
	inpic->width = _codec_context->width;
	inpic->height = _codec_context->height;
	auto success = av_image_fill_arrays(inpic->data, inpic->linesize, _current_frame.data.data(), AV_PIX_FMT_RGB24, _current_frame.info.width, _current_frame.info.height, 32);

	AVFrame* outpic = av_frame_alloc();
	outpic->format = AV_PIX_FMT_RGB24;
	outpic->width = _codec_context->width;
	outpic->height = _codec_context->height;
	//outpic->pts = (int64_t)((float)i * (1000.0 / ((float)(_codec_context->time_base.den))) * 90);                              // setting frame pts
	//outpic->pts = av_frame_get_best_effort_timestamp(outpic);
	outpic->pts = _frame_counter++;
	av_image_alloc(outpic->data, outpic->linesize, _codec_context->width, _codec_context->height, _codec_context->pix_fmt, 32);

	while (!_quit_thread) {
		while (av_read_frame(_format_context, packet) >= 0)
		{
			if (packet->stream_index == _video_stream_index) {//packet is video
				std::cout << "2 Is Video" << std::endl;
				if (stream == nullptr)
				{//create stream in file
					std::cout << "3 create stream" << std::endl;
					stream = avformat_new_stream(oc, _format_context->streams[_video_stream_index]->codec->codec);
					avcodec_copy_context(stream->codec, _format_context->streams[_video_stream_index]->codec);
					stream->sample_aspect_ratio = _format_context->streams[_video_stream_index]->codec->sample_aspect_ratio;
				}
				int check = 0;
				packet->stream_index = stream->id;
				std::cout << "4 decoding" << std::endl;
				int result = avcodec_decode_video2(_codec_context, inpic, &check, packet);
				std::cout << "Bytes decoded " << result << " check " << check << std::endl;

				sws_scale(_scale_context, inpic->data, inpic->linesize, 0, _codec_context->height, outpic->data, outpic->linesize);
				decode_frame(outpic, _codec_context);
			}
			av_packet_free(&packet);
			av_init_packet(packet);
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(frame_rate));
	}

	av_free(outpic);
	av_free(inpic);

	//av_read_pause(_format_context);
	//avio_close(oc->pb);
	avformat_free_context(oc);

}

