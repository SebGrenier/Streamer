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
9. from shell run "./configure --enable-asm --enable-yasm --arch=x86_64 --disable-programs --disable-doc --enable-static --enable-libx264 --disable-bzlib --disable-libopenjpeg --disable-iconv --disable-zlib --disable-network --enable-gpl --enable-nonfree --enable-nvenc --prefix=/d/Projets/ffmpeg-build --toolchain=msvc"
10. run "make" to build
11. run "make install" to install in /d/Projets/ffmpeg-build
