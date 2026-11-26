#!/bin/bash
# The Android toolchain, pinned (EXPORT-APK: a game exported as an .apk for the player's own device, built on their
# desktop): Google's NDK (clang, the Android C library and its headers), the SDK's build-tools (aapt2, d8, zipalign,
# apksigner) and platform (android.jar) for packaging, a JDK for those Java tools, and per ABI -- arm64-v8a for
# devices, x86_64 for the emulator -- SDL3 (libSDL3.so, the same release the desktop builds use) and zlib built with the
# NDK; SDL's Java side (SDLActivity and its helpers) compiled once into sdl-java/ (classes.jar to build against,
# classes.dex to ship).
# Into $ATIV_ANDROID_TOOLCHAIN (default ~/toolchains/ativ-android).
#   setup-toolchain.sh        on a Linux x86_64 machine (or WSL) with curl, python3, cmake and ninja or make
# The SDK parts are Google's, under the Android Software Development Kit License Agreement
# (https://developer.android.com/studio/terms); they stay on this machine, nothing of them is shipped but what an APK holds.
set -euo pipefail
NDK=r30                    # ndk;30.0.16248370 (repository2-3.xml: sha1 5107f898313790e449e87eee2183d9a20602dee9)
NDK_SHA=753611f410d002cfcd3f3dc2ef49aad532089d3180b436c060a90bf0fcb64df2
BUILD_TOOLS=36.0.0         # build-tools;36.0.0 (sha1 b0b6376977657e8ad9b969bacf4093601da2c6fb)
BUILD_TOOLS_ZIP=build-tools_r36_linux.zip
BUILD_TOOLS_SHA=5d9ac77fb6ff43d9da518a337b4fcf8f9097113df531d99ccefe80ef7ce8250b
PLATFORM=36                # platforms;android-36 (sha1 2c1a80dd4d9f7d0e6dd336ec603d9b5c55a6f576)
PLATFORM_ZIP=platform-36_r02.zip
PLATFORM_SHA=37607369a28c5b640b3a7998868d45898ebcb777565a0e85f9acf36f29631d2e
JDK=21.0.13+9              # Eclipse Temurin
JDK_TGZ=OpenJDK21U-jdk_x64_linux_hotspot_21.0.13_9.tar.gz
JDK_SHA=d7f5d5cf865a2902b70ff549b97586a7d3f234e016f041ab447ea740c7d66d1c
SDL=3.4.16
SDL_SHA=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
ZLIB=1.3.1
ZLIB_SHA=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
FFMPEG=7.1.1               # (the Linux script's: H.264 + AAC decoders for the movies, EXPORT-APK A9)
FFMPEG_SHA=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
API=29                     # the oldest Android a game runs on: 10 (64-bit only, Vulkan 1.1 on new devices)
ABIS="arm64-v8a x86_64"
D=${ATIV_ANDROID_TOOLCHAIN:-$HOME/toolchains/ativ-android}
HERE=$(cd "$(dirname "$0")" && pwd)   # (runtime/platform/android: rawdec.c is in ../ffmpeg)
mkdir -p "$D/dl"; cd "$D/dl"
verify() { local h; h=$(sha256sum "$1" | awk '{print $1}'); [[ $h == "$2" ]] && return; echo "checksum mismatch: $1 (expected $2, got $h)"; rm -f "$1"; exit 1; }
fetch() { [[ -f $2 ]] || { curl -fsSL --retry 5 --retry-all-errors -o "$2.part" "$1" && verify "$2.part" "$3" && mv -f "$2.part" "$2"; }; }
fetch "https://dl.google.com/android/repository/android-ndk-$NDK-linux.zip" "android-ndk-$NDK-linux.zip" "$NDK_SHA"
fetch "https://dl.google.com/android/repository/$BUILD_TOOLS_ZIP" "$BUILD_TOOLS_ZIP" "$BUILD_TOOLS_SHA"
fetch "https://dl.google.com/android/repository/$PLATFORM_ZIP" "$PLATFORM_ZIP" "$PLATFORM_SHA"
fetch "https://github.com/adoptium/temurin21-binaries/releases/download/jdk-${JDK/+/%2B}/$JDK_TGZ" "$JDK_TGZ" "$JDK_SHA"
fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL/SDL3-$SDL.tar.gz" "SDL3-$SDL.tar.gz" "$SDL_SHA"
fetch "https://github.com/madler/zlib/releases/download/v$ZLIB/zlib-$ZLIB.tar.gz" "zlib-$ZLIB.tar.gz" "$ZLIB_SHA"
fetch "https://ffmpeg.org/releases/ffmpeg-$FFMPEG.tar.xz" "ffmpeg-$FFMPEG.tar.xz" "$FFMPEG_SHA"
cd "$D"
# a zip's one top folder as <dest>, keeping the programs' modes and the symlinks (no unzip needed)
unzip_to() {
  rm -rf "$2" "$2.tmp"; mkdir -p "$2.tmp"
  python3 - "$1" "$2.tmp" <<'PY'
import os, stat, sys, zipfile
src, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(src) as z:
    for i in z.infolist():
        mode = (i.external_attr >> 16) & 0xFFFF
        p = os.path.join(out, i.filename)
        if stat.S_ISLNK(mode):
            os.makedirs(os.path.dirname(p), exist_ok=True)
            os.symlink(z.read(i).decode(), p)
            continue
        z.extract(i, out)
        if mode and not i.is_dir():
            os.chmod(p, stat.S_IMODE(mode))
PY
  local top; top=$(ls "$2.tmp"); [[ $(echo "$top" | wc -l) == 1 ]] || { echo "unzip_to: $1 has more than one top folder"; exit 1; }
  mkdir -p "$(dirname "$2")"; mv "$2.tmp/$top" "$2"; rmdir "$2.tmp"
}
[[ -x ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/clang ]] || unzip_to "dl/android-ndk-$NDK-linux.zip" ndk
[[ -x sdk/build-tools/$BUILD_TOOLS/d8 ]] || unzip_to "dl/$BUILD_TOOLS_ZIP" "sdk/build-tools/$BUILD_TOOLS"
[[ -f sdk/platforms/android-$PLATFORM/android.jar ]] || unzip_to "dl/$PLATFORM_ZIP" "sdk/platforms/android-$PLATFORM"
[[ -x jdk/bin/javac ]] || { rm -rf jdk; mkdir jdk; tar xzf "dl/$JDK_TGZ" -C jdk --strip-components 1; }
export JAVA_HOME=$D/jdk PATH=$D/jdk/bin:$PATH
LLVM=$D/ndk/toolchains/llvm/prebuilt/linux-x86_64
JAR=$D/sdk/platforms/android-$PLATFORM/android.jar
# the compilers per ABI as plain programs (build.py takes one program): bin/<abi>-cc, -c++, -ar
mkdir -p bin
for abi in $ABIS; do
  case $abi in arm64-v8a) t=aarch64-linux-android;; x86_64) t=x86_64-linux-android;; esac
  for n in cc:clang c++:clang++; do
    printf '#!/bin/sh\nD=$(cd "$(dirname "$0")/.." && pwd)\nexec "$D/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/%s%s-%s" "$@"\n' "$t" "$API" "${n#*:}" > "bin/$abi-${n%%:*}"
  done
  printf '#!/bin/sh\nD=$(cd "$(dirname "$0")/.." && pwd)\nexec "$D/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar" "$@"\n' > "bin/$abi-ar"
  chmod +x bin/$abi-*
done
# Nativ's patches to the pinned SDL release (runtime/platform/sdl-patches/README.md: the Vulkan pipeline cache file,
# ANDROID-PERF-03); a change to them extracts SDL afresh and rebuilds it for every ABI
PATCHES=$HERE/../sdl-patches
PSTAMP=$(cat "$PATCHES"/*.patch "$PATCHES"/*.h | tr -d '\r' | sha256sum | cut -c1-16)
if [[ ! -d sdl-src || $(cat sdl-src/.nativ-patches 2>/dev/null) != "$PSTAMP" ]]; then
  rm -rf sdl-src sdl-src.tmp; mkdir sdl-src.tmp; tar xzf "dl/SDL3-$SDL.tar.gz" -C sdl-src.tmp --strip-components 1
  cp "$PATCHES"/*.h sdl-src.tmp/src/gpu/vulkan/
  for p in "$PATCHES"/*.patch; do patch -d sdl-src.tmp -p1 --quiet --forward < "$p" || { echo "SDL patch $p does not apply to SDL $SDL"; exit 1; }; done
  echo "$PSTAMP" > sdl-src.tmp/.nativ-patches; mv sdl-src.tmp sdl-src
  for abi in $ABIS; do rm -rf "$abi/SDL3"; done
fi
[[ -d zlib-src ]] || { mkdir zlib-src.tmp; tar xzf "dl/zlib-$ZLIB.tar.gz" -C zlib-src.tmp --strip-components 1; mv zlib-src.tmp zlib-src; }
GEN=; command -v ninja >/dev/null && GEN="-G Ninja"
for abi in $ABIS; do
  if [[ ! -f $abi/zlib/lib/libz.a ]]; then
    rm -rf "zlib-build-$abi"; mkdir -p "zlib-build-$abi" "$abi/zlib/lib" "$abi/zlib/include"
    ( cd "zlib-build-$abi" && for f in adler32 crc32 deflate infback inffast inflate inftrees trees zutil compress uncompr gzclose gzlib gzread gzwrite; do
        "$D/bin/$abi-cc" -O2 -fPIC -D_LARGEFILE64_SOURCE=1 -DHAVE_UNISTD_H -c "$D/zlib-src/$f.c" -o $f.o; done
      "$D/bin/$abi-ar" rcs "$D/$abi/zlib/lib/libz.a" *.o )
    cp zlib-src/zlib.h zlib-src/zconf.h "$abi/zlib/include/"
  fi
  if [[ ! -f $abi/SDL3/lib/libSDL3.so ]]; then
    rm -rf "sdl-build-$abi"
    cmake -S sdl-src -B "sdl-build-$abi" $GEN -DCMAKE_TOOLCHAIN_FILE="$D/ndk/build/cmake/android.toolchain.cmake" \
      -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$API" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$D/$abi/SDL3" -DCMAKE_INSTALL_LIBDIR=lib \
      -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL_DOCS=OFF > "sdl-configure-$abi.log"
    cmake --build "sdl-build-$abi" -j"$(nproc)" > "sdl-build-$abi.log" 2>&1 && cmake --install "sdl-build-$abi" >> "sdl-build-$abi.log" 2>&1
  fi
  # FFmpeg's H.264 and AAC decoders (libavcodec + libavutil, static, position-independent and with hidden symbols: linked
  # into libmain.so -- its arm64 assembly addresses tables such as ff_tx_tab_32_float directly, which a shared library
  # allows only for symbols it does not export: "relocation R_AARCH64_ADR_PREL_PG_HI21 cannot be used against symbol") and the
  # AAC raw-frame shim, as the Linux and Windows toolchains have them (runtime/media/video.c's sceAvcdec: the movies)
  if [[ ! -f $abi/ffmpeg/lib/libavcodec.a || ! -f $abi/ffmpeg/lib/libnativ_rawdec.a ]]; then
    case $abi in arm64-v8a) arch=aarch64; asm=;; x86_64) arch=x86_64; asm=--disable-x86asm;; esac
    rm -rf "$abi/ffmpeg" "ffmpeg-src-$abi"; mkdir "ffmpeg-src-$abi"; tar xJf "dl/ffmpeg-$FFMPEG.tar.xz" -C "ffmpeg-src-$abi" --strip-components 1
    ( cd "ffmpeg-src-$abi" && ./configure --prefix="$D/$abi/ffmpeg" --enable-cross-compile --target-os=android --arch=$arch \
        --cc="$D/bin/$abi-cc" --cxx="$D/bin/$abi-c++" --ar="$LLVM/bin/llvm-ar" --ranlib="$LLVM/bin/llvm-ranlib" --nm="$LLVM/bin/llvm-nm" --strip="$LLVM/bin/llvm-strip" \
        --enable-static --disable-shared --enable-pic --disable-everything --disable-programs --disable-doc --disable-network --disable-autodetect \
        --disable-avdevice --disable-avformat --disable-swscale --disable-swresample --disable-avfilter --disable-debug \
        --enable-decoder=h264 --enable-parser=h264 --enable-parser=hevc --enable-decoder=aac $asm --extra-cflags=-fvisibility=hidden > "../ffmpeg-configure-$abi.log" \
      && make -j"$(nproc)" > "../ffmpeg-build-$abi.log" 2>&1 && make install >> "../ffmpeg-build-$abi.log" 2>&1 \
      && "$D/bin/$abi-cc" -O2 -fPIC -I. -c "$HERE/../ffmpeg/rawdec.c" -o rawdec.o && "$LLVM/bin/llvm-ar" rcs "$D/$abi/ffmpeg/lib/libnativ_rawdec.a" rawdec.o )
    [[ -f $abi/ffmpeg/lib/libavcodec.a ]] || { echo "FFmpeg for $abi failed: $D/ffmpeg-build-$abi.log"; exit 1; }
  fi
done
# SDL's Java side, once: compiled against android.jar (classes.jar: what an app's own Activity builds against), then d8
# into classes.dex (what an APK carries)
if [[ ! -f sdl-java/classes.dex ]]; then
  rm -rf sdl-java; mkdir -p sdl-java/classes
  find sdl-src/android-project/app/src/main/java -name '*.java' > sdl-java/sources.txt
  javac -nowarn --release 11 -encoding UTF-8 -classpath "$JAR" -d sdl-java/classes @sdl-java/sources.txt 2> sdl-java/javac.log
  ( cd sdl-java/classes && "$JAVA_HOME/bin/jar" cf ../classes.jar . )
  "sdk/build-tools/$BUILD_TOOLS/d8" --release --min-api "$API" --lib "$JAR" --output sdl-java sdl-java/classes.jar
fi
# their licences beside them (an APK ships libSDL3.so and the dex)
mkdir -p share/licenses
cp sdl-src/LICENSE.txt share/licenses/SDL3-LICENSE.txt; cp zlib-src/LICENSE share/licenses/zlib-LICENSE.txt
src1=(ffmpeg-src-*); cp "${src1[0]}/LICENSE.md" share/licenses/FFmpeg-LICENSE.md; cp "${src1[0]}/COPYING.LGPLv2.1" share/licenses/FFmpeg-COPYING.LGPLv2.1
printf 'FFmpeg %s (LGPL 2.1 or later), unmodified: https://ffmpeg.org/releases/ffmpeg-%s.tar.xz\nbuilt by runtime/platform/android/setup-toolchain.sh with only the H.264 and AAC decoders and the parsers enabled, plus libnativ_rawdec.a (runtime/platform/ffmpeg/rawdec.c).\n' "$FFMPEG" "$FFMPEG" > share/licenses/FFmpeg-SOURCE.txt
# check: a small libmain.so per ABI that links SDL3 and zlib, as an APK's code will
mkdir -p check
cat > check/main.c <<'C'
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <zlib.h>
int main(int argc, char **argv) { (void)argc; (void)argv; SDL_Log("nativ android check, zlib %s", zlibVersion()); return 0; }
C
ok=
for abi in $ABIS; do
  "$D/bin/$abi-cc" -O2 -fPIC -shared -I"$abi/SDL3/include" -I"$abi/zlib/include" check/main.c -o "check/libmain-$abi.so" \
    -L"$abi/SDL3/lib" -lSDL3 "$abi/zlib/lib/libz.a" -llog -Wl,-z,max-page-size=16384
  ok="$ok $abi:$("$LLVM/bin/llvm-readelf" -h "check/libmain-$abi.so" | awk -F: '/Machine/ {gsub(/^ +/,"",$2); print $2}' | tr ' ' '_')"
done
echo "toolchain: $D (NDK $NDK clang $("$LLVM/bin/clang" --version | head -1 | sed 's/.*clang version \([0-9.]*\).*/\1/'), API $API; build-tools $BUILD_TOOLS, android-$PLATFORM; JDK $("$JAVA_HOME/bin/java" -version 2>&1 | head -1 | sed 's/.*"\(.*\)".*/\1/'); SDL3 $SDL (patches $(cat sdl-src/.nativ-patches)); zlib $ZLIB; FFmpeg $FFMPEG $(ls */ffmpeg/lib/libavcodec.a 2>/dev/null | wc -l)/2 ABIs; sdl-java $(ls sdl-java/classes.dex >/dev/null 2>&1 && echo classes.dex || echo MISSING); check$ok)"
