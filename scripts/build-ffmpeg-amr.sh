#!/usr/bin/env bash
# Builds a tiny static FFmpeg (LGPL) with only the AMR-NB and AMR-WB decoders,
# the vitasdk FFmpeg package leaves them out. Headers land in <dest>/include,
# libraries in <dest>/lib, linked by path so they never mix with the vitasdk ones.
# Usage: build-ffmpeg-amr.sh <dest-dir>
set -euo pipefail
dest="$(mkdir -p "${1:?destination dir}" && cd "$1" && pwd)"
ver=7.1
[ -f "$dest/lib/libavcodec.a" ] && [ "$(cat "$dest/ver" 2>/dev/null)" = "$ver" ] && exit 0
src="$dest/ffmpeg-$ver"
if [ ! -d "$src" ]; then
  curl -sL "https://ffmpeg.org/releases/ffmpeg-$ver.tar.xz" | tar -xJ -C "$dest"
fi
cd "$src"
./configure --prefix="$dest" --enable-cross-compile --cross-prefix=arm-vita-eabi- \
  --arch=arm --cpu=cortex-a9 --target-os=none --enable-static --disable-shared \
  --disable-runtime-cpudetect --disable-armv5te --disable-programs --disable-doc \
  --disable-network --disable-everything --disable-avdevice --disable-avformat \
  --disable-swscale --disable-swresample --disable-avfilter --disable-pthreads \
  --disable-iconv --disable-zlib --disable-bzlib --disable-lzma \
  --enable-decoder=amrnb,amrwb --extra-cflags="-O2 -mfpu=neon -Wno-error=incompatible-pointer-types -Wno-error=int-conversion" >/dev/null
make -j"$(nproc)" >/dev/null
make install >/dev/null
echo "$ver" > "$dest/ver"
