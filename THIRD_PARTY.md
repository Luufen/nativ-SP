# Third-party software

Project Nativ's own code is free software under the GNU General Public License, version 2 or (at your option) any
later version (`LICENSE`). It includes or installs the software below, and parts of it are ported from the projects
listed under "Ported from". Each keeps its own licence, whose text travels with it.

## In this repository

| Software | Version | Licence | Where |
|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.9b | MIT | `third_party/imgui/` (`LICENSE.txt`) |
| [stb_image and stb_image_write](https://github.com/nothings/stb) | 2.28 and 1.16 | MIT or public domain, your choice | `third_party/stb/` (the licence closes each header) |
| [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9) | | MIT | `third_party/libatrac9/` (`LICENSE`) |
| [Monocypher](https://monocypher.org) | 4.0.2 | BSD 2-clause or CC0, your choice | `third_party/monocypher/` (`LICENCE.md`) |
| [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK): FSR 3.1's upscaler shaders | v1.1.4 | MIT | `third_party/fidelityfx/` (`LICENSE.txt`); compiled into `runtime/gfx/fsr_shaders.h` |
| HE-VAG coefficients from [vgmstream](https://github.com/vgmstream/vgmstream) | | ISC | `third_party/hevag/hevag_coefs.h` (in its header) |
| [Input Prompts](https://kenney.nl/assets/input-prompts) by Kenney: the touch card's button icons | 1.5A | CC0 1.0 | `third_party/kenney-prompts/` (`License.txt`; `make.py` packs `prompts.png` from it) |
| [xxHash](https://github.com/Cyan4973/xxHash) | 0.8.5 | BSD 2-clause | `third_party/xxhash/` (`LICENSE`); XXH3, for Vita3K-style texture pack hashes (`runtime/gfx/texpack.c`) |

## Ported from

Written for Nativ in Python after these projects' code, which they credit in their headers.

| Nativ | Ported from | Licence |
|---|---|---|
| `decrypt/vitadecrypt/` `keys.py`, `sce_self.py`, `pkg.py`, `pup.py`, `sfo.py` | [Vita3K](https://github.com/Vita3K/Vita3K)'s `packages/` | GPL 2.0 or later |
| the key values and SELF/PUP handling Vita3K credits | [TeamMolecule's sceutils](https://github.com/TeamMolecule/sceutils) | MIT |
| the PKG format Vita3K credits | [pkg2zip](https://github.com/mmozeiko/pkg2zip) | Unlicense |
| `decrypt/vitadecrypt/pfs.py` | [psvpfstools](https://github.com/motoharu-gosuto/psvpfstools) and Vita3K's [psvpfsparser](https://github.com/Vita3K/psvpfsparser) | none published |
| `decrypt/nidmap.tsv` (function names by NID) | [VitaSDK's NID database](https://github.com/vitasdk/vita-headers) | MIT |

## Installed with Nativ

The install scripts (`dev/macos-install.sh`, `dev/linux-install.sh`, `dev/windows-install.sh`) add these, so a
player's machine needs nothing else. Each sits in the install with its licence (on a Mac the install is inside the app:
`Project Nativ.app/Contents/Resources/`).

| Software | Version | Licence | Where in an install |
|---|---|---|---|
| [SDL](https://libsdl.org) | 3.4.16 | zlib | `toolchain/SDL3/share/licenses/SDL3/` |
| [FFmpeg](https://ffmpeg.org): libavcodec and libavutil, the H.264 and AAC decoders only (plus `runtime/platform/ffmpeg/rawdec.c`, built against it) | 7.1.1 | LGPL 2.1 or later | `toolchain/ffmpeg/share/licenses/`, with where its unmodified source is |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) (macOS) | vulkan-sdk-1.4.363.0 | Apache 2.0 | `toolchain/spirv-cross/share/licenses/` |
| [Zig](https://ziglang.org), its Clang and LLVM (Linux) | 0.15.2 | MIT; LLVM: Apache 2.0 with LLVM exceptions | `toolchain/zig/` |
| [zlib](https://zlib.net) (Linux, Windows) | 1.3.1 | zlib | `toolchain/zlib/share/licenses/` |
| [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), its Clang, LLVM and mingw-w64 (Windows) | 20261006 | ISC; LLVM: Apache 2.0 with LLVM exceptions; mingw-w64: ZPL and public domain | `toolchain/llvm-mingw/` |
| [AppImage's runtime](https://github.com/AppImage/type2-runtime) (Linux) | 20251108 | MIT; it includes libfuse 3 (LGPL 2.1) and squashfuse (BSD 2-clause) | the start of the `.AppImage` file |
| [Python](https://www.python.org), from [python-build-standalone](https://github.com/astral-sh/python-build-standalone) | 3.13.17 | PSF, and its parts' own | `python/` |
| [Capstone](https://www.capstone-engine.org) | 5.0.9 | BSD 3-clause | `python/`, installed by pip with its licence |
| [PyCryptodome](https://www.pycryptodome.org) | 3.24.0 | BSD 2-clause, parts public domain | `python/`, installed by pip with its licence |

Nativ links FFmpeg's libraries into each game on the player's own machine, from these unmodified builds. On macOS,
Apple's Command Line Tools (the compiler and SDK) come from Apple and are not part of Nativ.

## For the Android export, fetched on the player's machine

`runtime/platform/android/setup-toolchain.sh` downloads and builds these into `~/toolchains/ativ-android` (Linux or WSL)
when the player sets up the Android export; none of them is part of a Nativ release. An exported `.apk` holds the
compiled game with SDL3, zlib and FFmpeg's decoders built in (and SDL's Java side), for the player's own device only.

| Software | Version | Licence |
|---|---|---|
| [Android NDK](https://developer.android.com/ndk) | r30 | Android Software Development Kit License Agreement; its parts' own (LLVM: Apache 2.0 with LLVM exceptions) |
| Android SDK build-tools and platform | 36.0.0 / android-36 | Android Software Development Kit License Agreement |
| [Eclipse Temurin](https://adoptium.net) JDK | 21.0.13+9 | GPL 2.0 with the Classpath Exception (the build's tools only, nothing of it is in an APK) |
| [SDL](https://libsdl.org), libSDL3.so and its Java side | 3.4.16 | zlib |
| [zlib](https://zlib.net) | 1.3.1 | zlib |
| [FFmpeg](https://ffmpeg.org): libavcodec and libavutil, the H.264 and AAC decoders only | 7.1.1 | LGPL 2.1 or later |
| [libadrenotools](https://github.com/bylaws/libadrenotools), ARM64 Android APKs | `8fae8ce254dfc1344527e05301e43f37dea2df80` | BSD 2-clause |
| [liblinkernsbypass](https://github.com/bylaws/liblinkernsbypass), libadrenotools dependency | `aa3975893d83ef1bc84c321ec60c65fbf1287887` | BSD 2-clause |

ARM64 exports build the GPU driver loader from the pinned sources and include their notices under `assets/licenses/`.
The Android app imports driver packages selected by the player into its private storage.
