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
} // end extern "C"

AVCodecID get_codec_id(CODEC codec) {
    switch (codec) {
        case H264:
            return AV_CODEC_ID_H264;
        default:
            return AV_CODEC_ID_H264;
    }
}

Decoder::Decoder(CODEC codec)
{
    av_register_all();
    avcodec_register_all();
    _codec = avcodec_find_decoder(get_codec_id(codec));
    _codec_context = avcodec_alloc_context3(_codec);
}

Decoder::~Decoder()
{
    avcodec_close(_codec_context);
}

void Decoder::decode(unsigned char *data, unsigned length)
{

}

void Decoder::parse_nal(unsigned char *data, unsigned length)
{
    if (length == 0){
        return;
    };

    var hit = function(subarray){
        if (subarray){
            bufferAr.push(subarray);
        };
        var buff = concatUint8(bufferAr);
        player.decode(buff);
        bufferAr = [];
    };

    var b = 0;
    var lastStart = 0;

    var l = data.length;
    var zeroCnt = 0;

    for (b = 0; b < l; ++b){
        if (data[b] === 0){
            zeroCnt++;
        }else{
            if (data[b] == 1){
                if (zeroCnt >= 3){
                    if (lastStart < b - 3){
                        hit(data.subarray(lastStart, b - 3));
                        lastStart = b - 3;
                    }else if (bufferAr.length){
                        hit();
                    }
                };
            };
            zeroCnt = 0;
        };
    };
    if (lastStart < data.length){
        bufferAr.push(data.subarray(lastStart));
    };
}

void Decoder::nal_hit(unsigned char *data, unsigned length, unsigned offset){
    if (data != nullptr){
        bufferAr.push(subarray);
    };
    decode(_buffer.data(), _buffer.size());
    _buffer.clear();
}
