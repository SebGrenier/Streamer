cmake_minimum_required(VERSION 3.6)
project(DecoderJs)

set(CMAKE_CXX_STANDARD 11)

set(BUILD_DIST ./build/dist/)
set(EMSCRIPTEN_DIR ENV{EMSCRIPTEN})

include_directories(${BUILD_DIST}include/)
include_directories(${EMSCRIPTEN_DIR}/system/include/)

set(SOURCE_FILES
    decoder-worker.js
    Decoder.cpp
    Decoder.h
    embinder.cpp
    Gruntfile.js)

add_executable(DecoderJs ${SOURCE_FILES})