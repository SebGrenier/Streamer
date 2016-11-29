# Streamer

## Building libx264
VS2015:

1. MSYS and MSVS 2015
2. run "VS2015 x86 Native Tools Command Prompt" or "VS2015 x64 Native Tools Command Prompt" depending what version (32 or 64-bit) you want to build
3. cd C:\msys64 and run mingw64.exe
4. Make sure to export path to cl and yasm (general x64 yasm, not vsyasm) like this :
 - export PATH="/c/Program Files (x86)/Microsoft Visual Studio 14.0/VC/bin/amd64":$PATH
 - export PATH="/d/Projets/vsyasm-1.3.0-win64":$PATH
5. change dir to x264 path
6. from shell run "CC=cl ./configure --disable-cli --enable-static --extra-cflags="-DNO_PREFIX"" for x264 configuring
7. run "make" which should build libx264.lib usable from MSVS
8. run "make install-lib-static" to install into /usr/local inside msys

P.S. MSVS builds would be a little bit slower than one build by MinGW

## Building ffmpeg
VS2015:

1. MSYS, MSVS 2015 and NVidia Video Codec SDK.
2. Copy Video_Codec_SDK_7.0.1\Samples\common\inc into /usr/local/include
3. run "VS2015 x86 Native Tools Command Prompt" or "VS2015 x64 Native Tools Command Prompt" depending what version (32 or 64-bit) you want to build
4. run "vcvarsall.bat amd64"
5. "cd C:\msys64" and run mingw64.exe
6. Make sure to export path to cl and yasm (general x64 yasm, not vsyasm) like this :
 - export PATH="/c/Program Files (x86)/Microsoft Visual Studio 14.0/VC/bin/amd64":$PATH
 - export PATH="/d/Projets/vsyasm-1.3.0-win64":$PATH
7. Make sure to export path to includes and libs, like this:
 - export INCLUDE="$INCLUDE;C:\msys64\usr\local\include"
 - export LIB="$LIB;C:\msys64\usr\local\lib"
8. change dir to ffmpeg
9. from shell run
 - "./configure --enable-asm --enable-yasm --arch=x86_64 --disable-programs --disable-doc --enable-static --enable-libx264 --disable-bzlib --disable-libopenjpeg --disable-iconv --disable-zlib --disable-network --enable-gpl --enable-nonfree --enable-nvenc --prefix=/d/Projets/ffmpeg-build --toolchain=msvc"
 - or for debug
 - "./configure --enable-asm --enable-yasm --arch=x86_64 --disable-programs --disable-doc --enable-static --enable-libx264 --disable-bzlib --disable-libopenjpeg --disable-iconv --disable-zlib --disable-network --enable-gpl --enable-nonfree --enable-nvenc --disable-optimizations --enable-debug --disable-stripping --prefix=/d/Projets/ffmpeg-build --toolchain=msvc"
10. run "make" to build
11. run "make install" to install in /d/Projets/ffmpeg-build

# DecoderJs
Modified version of Codecbox.js (https://github.com/duanyao/codecbox.js) to decode H264 frames. The decoder takes care of finding the nal units (https://en.wikipedia.org/wiki/Network_Abstraction_Layer) for you. You receive the decoded frame as an RGBA UInt8Array.

## Build
You need a Linux or similar system to build decoder.js.

1. Install [emscripten](http://kripken.github.io/emscripten-site/docs/getting_started/index.html), the C/C++ to asm.js comipler. Node.js should be installed as well.

2. Install [grunt](http://gruntjs.com/), the build runner for JS.

3. Clone Streamer repostory.

4. Go to `Streamer/DecoderJs` dir in a terminal, execute command `npm install`. Do the following steps in this dir as well.

5. Execute command `grunt init`. This clones repos of ffmpeg and its external libraries (including [x264](git://git.videolan.org/x264.git), [openh264](https://github.com/cisco/openh264.git), [libvpx](https://github.com/webmproject/libvpx.git), [libopus](https://github.com/xiph/opus.git), [lame](https://github.com/rbrito/lame.git), and [zlib](https://github.com/madler/zlib.git)), so make sure you have good internet connection.
   This project try to build the latest master branch of these libraries. If you want to stick to specific versions, go to `build/<lib_name>` and checkout manually. You may also want to adjust and apply patches in `patch` dir.

6. Execute command `grunt build`. This should compile ffmpeg etc. and produces `decoder.js` and `decoder.js.mem` in `src` dir.

## Customization
You can customize the build of ffmpeg. Open file `Gruntfile.js`, and edit
`ffDecoders`, `ffDemuxers`, `ffParsers`, `ffEncoders`, `ffMuxers` and `ffFilters` to select components of ffmpeg.
You may also toggle comment of the following 2 lines to enable full ffmpeg build:

```
configure: ffmpegCustomConfig,
//configure: ffmpegFullConfig,
```

Currently the full ffmpeg build includes all its buildin components, as well as external libraries. Default custom build also include these external libraries.

## API
Currently `codecbox.js` exposes a class `Module.Decoder` for video/audio decoding. See `Decoder.h` for its API. Note that C++ methods `get_XXX()/set_XXX()` are mapped to JS properties `XXX`. See also `decoder-worker.js` for its usage.

## Acknowledgement
Codecbox.js : https://github.com/duanyao/codecbox.js

