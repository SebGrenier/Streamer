#include "Decoder.h"

#include <emscripten/bind.h>

using namespace emscripten;

EMSCRIPTEN_BINDINGS(decoder)
{
    enum_<CODEC>("CODEC")
        .value("H264", CODEC::H264)
        ;

    class_<Decoder>("Decoder")
        .constructor<CODEC>()
        .function("decode", &Decoder::decode, allow_raw_pointers())
        .function("getFrame", &Decoder::get_frame)
        .property("hasFrame", &Decoder::get_hasFrame)
        .property("width", &Decoder::get_width)
        .property("height", &Decoder::get_height)
        ;
}

