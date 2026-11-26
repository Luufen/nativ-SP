# Nativ's patches to SDL

The Android and Linux toolchains (`runtime/platform/{android,linux}/setup-toolchain.sh`) build SDL from the pinned release
tarball (SDL 3.4.16, `SDL3-3.4.16.tar.gz` SHA-256 `7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68`). They copy
every `*.h` here into SDL's `src/gpu/vulkan/`, apply every `*.patch` here in name order (`patch -p1`), and record a stamp of
these files; a change here makes the next toolchain run extract and build SDL afresh. Windows (the prebuilt SDL3 mingw
package) and macOS (Metal) use SDL unpatched; Nativ's requests to a patched SDL are properties a stock SDL ignores.

SDL is zlib-licensed; these patches are Nativ's own, under Nativ's licence.

## 0001-vulkan-pipeline-cache.patch + nativ_vk_pcache.h (ANDROID-PERF-03)

SDL_GPU's Vulkan backend creates every pipeline with `VK_NULL_HANDLE` as its cache, and SDL has no API for one (SDL main
`3ebae2a704f945edd51e204d96a059d3946cb58e`, 2026-11-25: still `VK_NULL_HANDLE`; upstream's design discussion is
libsdl-org/SDL issue 11629, open). A driver without a disk cache of its own (Qualcomm's Adreno driver) therefore compiles
every pipeline again in every session.

The patch gives the backend one `VkPipelineCache` for all graphics and compute pipelines:

- At device creation it reads the file named by the string property `SDL.gpu.device.create.vulkan.pipeline_cache_file`
  (else the environment variable `SDL_GPU_VULKAN_PIPELINE_CACHE_FILE`; neither: a cache in memory only). Nativ sets it
  (`gfx_gpu.c` `vk_pcache_file`: `shader_cache/vulkan_pipelines.bin`, `RECOMP_VK_PIPELINE_CACHE=<file>|0`).
- The file (`nativ_vk_pcache.h`) is a 40-byte header (magic, vendorID, deviceID, driverVersion, payload size, FNV-1a 64
  of the payload) and the driver's blob, whose own Vulkan header must match vendorID, deviceID and pipelineCacheUUID. A
  short, corrupt, truncated, foreign or other-driver file is ignored (the cache starts empty), and so is a blob the driver
  refuses. Tested by `runtime/tests/vk_pcache_test.c`.
- It writes the cache back atomically (`<file>.tmp`, then a rename): on a thread once pipeline creation has been quiet for
  2 s (at most every 15 s, and only if the driver's blob grew), synchronously on `SDL_EVENT_WILL_ENTER_BACKGROUND` and
  `SDL_EVENT_TERMINATING` (Android may end a backgrounded app without another word), and when the device is destroyed.
- It reports to stderr (Nativ's session log): `gpu: vulkan pipeline cache: loaded|starting empty|saved ...`.

Rebuild the patch from a pristine tarball and an edited copy:
`diff -u a/src/gpu/vulkan/SDL_gpu_vulkan.c b/src/gpu/vulkan/SDL_gpu_vulkan.c` with the file names as `a/src/...` and `b/src/...`.
