#!/usr/bin/env bash
set -euo pipefail

# APK packaging runs on Linux/WSL with the Android toolchain already installed.
at=$1
work=$2
src=$(cd -- "$(dirname -- "$0")" && pwd)
adreno=8fae8ce254dfc1344527e05301e43f37dea2df80
linker=aa3975893d83ef1bc84c321ec60c65fbf1287887
mkdir -p "$at/dl" "$work"

fetch() {
    local repo=$1 rev=$2 sha=$3 dest=$4
    local archive="$at/dl/$repo-$rev.tar.gz"
    if [[ ! -f "$archive" ]] || ! sha256sum --check --status <<< "$sha  $archive"; then
        local temp
        temp=$(mktemp "$at/dl/$repo-XXXXXXXX")
        if ! curl --fail --location --silent --show-error --retry 3 \
                "https://codeload.github.com/bylaws/$repo/tar.gz/$rev" -o "$temp" ||
            ! sha256sum --check --status <<< "$sha  $temp"; then
            rm -f -- "$temp"
            echo "Could not download or verify $repo at $rev" >&2
            return 1
        fi
        mv -f -- "$temp" "$archive"
    fi
    mkdir -p "$dest"
    tar xzf "$archive" -C "$dest" --strip-components=1
}

fetch libadrenotools "$adreno" ceffce971676d4cfdf348a082df06fc92a1dca6d95bea892a480d63f200961cb "$work/source"
fetch liblinkernsbypass "$linker" da1128c8aa771c4d24766b53a47822d0717baa5c536ca8491220402942b80638 "$work/source/lib/linkernsbypass"
cmake -S "$src" -B "$work/build" \
    -DCMAKE_TOOLCHAIN_FILE="$at/ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static \
    -DADRENOTOOLS_SOURCE="$work/source" -DCMAKE_BUILD_TYPE=Release
cmake --build "$work/build" --parallel --target nativ_vulkan main_hook
