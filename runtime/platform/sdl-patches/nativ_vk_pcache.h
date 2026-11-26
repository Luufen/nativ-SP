/* nativ_vk_pcache.h -- the on-disk Vulkan pipeline cache file (ANDROID-PERF-03). Nativ's own code, not SDL's: the
 * Android and Linux toolchains copy it into SDL's src/gpu/vulkan/ next to SDL_gpu_vulkan.c, and
 * 0001-vulkan-pipeline-cache.patch includes it there (sdl-patches/README.md); runtime/tests/vk_pcache_test.c includes the
 * same file. Plain C and stdio, no SDL and no Vulkan headers.
 *
 * Why: SDL_GPU creates every Vulkan pipeline with VK_NULL_HANDLE as its cache, so a driver without a disk cache of its
 * own (Adreno's proprietary one) compiles every pipeline again each session. The patch gives SDL one VkPipelineCache,
 * filled from this file at device creation and written back to it.
 *
 * The file: a 40-byte header of our own, then the driver's vkGetPipelineCacheData blob unchanged.
 *   0  "NATIVPC1"
 *   8  u32 vendorID, u32 deviceID, u32 driverVersion   (VkPhysicalDeviceProperties of the device that wrote it)
 *   20 u32 0 (reserved)
 *   24 u64 payload size (the file is exactly 40 + that)
 *   32 u64 FNV-1a 64 of the payload
 * all little-endian. The payload's own header (VkPipelineCacheHeaderVersionOne: u32 headerSize >= 32, u32 headerVersion 1,
 * u32 vendorID, u32 deviceID, u8 pipelineCacheUUID[16]) must match the device too. Anything else -- a short, truncated,
 * corrupt or foreign file, another GPU, a driver update -- is ignored and the cache starts empty: a driver handed a bad
 * blob may crash rather than reject it, so nothing unchecked reaches vkCreatePipelineCache.
 * Writing goes to <path>.tmp first, then a rename over <path> (MoveFileEx on Windows), so a crash or a kill mid-write
 * leaves the old file whole. */
#ifndef NATIV_VK_PCACHE_H
#define NATIV_VK_PCACHE_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#ifndef NVPC_MALLOC
#define NVPC_MALLOC malloc
#define NVPC_FREE free
#endif

#define NVPC_MAGIC "NATIVPC1"
#define NVPC_HDR 40
#define NVPC_VK_HDR 32                 /* sizeof(VkPipelineCacheHeaderVersionOne) */
#define NVPC_MAX_PAYLOAD (512u << 20)  /* a bigger file is not ours (or not worth reading) */

typedef struct { uint32_t vendor, device, driver; uint8_t uuid[16]; } NvpcDevice;   /* from VkPhysicalDeviceProperties */

enum { NVPC_OK, NVPC_NOFILE, NVPC_READ, NVPC_SHORT, NVPC_MAGIC_BAD, NVPC_SIZE, NVPC_TOOBIG, NVPC_SUM, NVPC_DRIVER, NVPC_VKHDR,
       NVPC_VENDOR, NVPC_DEVICE, NVPC_UUID };

static const char *nvpc_why(int r) {
	static const char *s[] = { "ok", "no file", "read error", "too short", "not a pipeline cache file", "size does not match",
	                           "too big", "checksum mismatch (corrupt)", "another driver version", "bad Vulkan cache header",
	                           "another GPU vendor", "another GPU", "another driver (pipelineCacheUUID)" };
	return r >= 0 && r < (int)(sizeof s / sizeof *s) ? s[r] : "?";
}

static uint32_t nvpc_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t nvpc_rd64(const uint8_t *p) { return (uint64_t)nvpc_rd32(p) | (uint64_t)nvpc_rd32(p + 4) << 32; }
static void nvpc_wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void nvpc_wr64(uint8_t *p, uint64_t v) { nvpc_wr32(p, (uint32_t)v); nvpc_wr32(p + 4, (uint32_t)(v >> 32)); }
static uint64_t nvpc_fnv(const uint8_t *p, size_t n) {
	uint64_t h = 0xcbf29ce484222325ull;
	for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; }
	return h;
}

/* the payload's own header against this device (also what a driver's blob must look like before we write it) */
static int nvpc_check_vk(const uint8_t *pl, size_t n, const NvpcDevice *dev) {
	if (n < NVPC_VK_HDR) return NVPC_VKHDR;
	uint32_t hsz = nvpc_rd32(pl), hver = nvpc_rd32(pl + 4);
	if (hsz < NVPC_VK_HDR || hsz > n || hver != 1) return NVPC_VKHDR;   /* (1: VK_PIPELINE_CACHE_HEADER_VERSION_ONE) */
	if (nvpc_rd32(pl + 8) != dev->vendor) return NVPC_VENDOR;
	if (nvpc_rd32(pl + 12) != dev->device) return NVPC_DEVICE;
	if (memcmp(pl + 16, dev->uuid, 16)) return NVPC_UUID;
	return NVPC_OK;
}

/* a whole file's bytes: NVPC_OK and the payload's place, or why not */
static int nvpc_check(const uint8_t *f, size_t n, const NvpcDevice *dev, size_t *pl_off, size_t *pl_n) {
	if (n < NVPC_HDR) return NVPC_SHORT;
	if (memcmp(f, NVPC_MAGIC, 8)) return NVPC_MAGIC_BAD;
	uint64_t psz = nvpc_rd64(f + 24);
	if (psz > NVPC_MAX_PAYLOAD) return NVPC_TOOBIG;
	if (psz != (uint64_t)(n - NVPC_HDR)) return NVPC_SIZE;
	if (nvpc_rd32(f + 8) != dev->vendor) return NVPC_VENDOR;
	if (nvpc_rd32(f + 12) != dev->device) return NVPC_DEVICE;
	if (nvpc_rd32(f + 16) != dev->driver) return NVPC_DRIVER;
	if (nvpc_fnv(f + NVPC_HDR, (size_t)psz) != nvpc_rd64(f + 32)) return NVPC_SUM;
	int r = nvpc_check_vk(f + NVPC_HDR, (size_t)psz, dev);
	if (r) return r;
	*pl_off = NVPC_HDR; *pl_n = (size_t)psz;
	return NVPC_OK;
}

static void nvpc_header(uint8_t h[NVPC_HDR], const NvpcDevice *dev, const void *payload, size_t n) {
	memcpy(h, NVPC_MAGIC, 8);
	nvpc_wr32(h + 8, dev->vendor); nvpc_wr32(h + 12, dev->device); nvpc_wr32(h + 16, dev->driver); nvpc_wr32(h + 20, 0);
	nvpc_wr64(h + 24, (uint64_t)n); nvpc_wr64(h + 32, nvpc_fnv((const uint8_t *)payload, n));
}

/* read and check <path>: NVPC_OK with *buf (NVPC_FREE it) holding the file, the payload at *buf + *pl_off; any other
 * result leaves *buf NULL */
static int nvpc_load(const char *path, const NvpcDevice *dev, void **buf, size_t *pl_off, size_t *pl_n, size_t *file_n) {
	*buf = NULL; *pl_off = *pl_n = 0; if (file_n) *file_n = 0;
	FILE *f = fopen(path, "rb");
	if (!f) return NVPC_NOFILE;
	long sz = -1;
	if (fseek(f, 0, SEEK_END) == 0) sz = ftell(f);
	if (sz < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NVPC_READ; }
	if (file_n) *file_n = (size_t)sz;
	if ((unsigned long)sz < NVPC_HDR) { fclose(f); return NVPC_SHORT; }
	if ((unsigned long)sz > NVPC_MAX_PAYLOAD + (unsigned long)NVPC_HDR) { fclose(f); return NVPC_TOOBIG; }
	uint8_t *b = (uint8_t *)NVPC_MALLOC((size_t)sz);
	if (!b) { fclose(f); return NVPC_READ; }
	size_t got = fread(b, 1, (size_t)sz, f);
	fclose(f);
	if (got != (size_t)sz) { NVPC_FREE(b); return NVPC_READ; }
	int r = nvpc_check(b, (size_t)sz, dev, pl_off, pl_n);
	if (r) { NVPC_FREE(b); return r; }
	*buf = b;
	return NVPC_OK;
}

/* write <path> atomically: 0, or -1 with <path> as it was. The payload must already pass nvpc_check_vk for dev */
static int nvpc_save(const char *path, const NvpcDevice *dev, const void *payload, size_t n) {
	if (n > NVPC_MAX_PAYLOAD || nvpc_check_vk((const uint8_t *)payload, n, dev)) return -1;
	size_t pl = strlen(path);
	char *tmp = (char *)NVPC_MALLOC(pl + 5);
	if (!tmp) return -1;
	memcpy(tmp, path, pl); memcpy(tmp + pl, ".tmp", 5);
	uint8_t h[NVPC_HDR]; nvpc_header(h, dev, payload, n);
	FILE *f = fopen(tmp, "wb");
	int ok = f != NULL;
	if (ok) ok = fwrite(h, 1, NVPC_HDR, f) == NVPC_HDR && (n == 0 || fwrite(payload, 1, n, f) == n) && fflush(f) == 0;
#ifdef _WIN32
	if (ok) ok = _commit(_fileno(f)) == 0;
#else
	if (ok) ok = fsync(fileno(f)) == 0;   /* (the data on disk before the rename makes it the file) */
#endif
	if (f && fclose(f) != 0) ok = 0;
#ifdef _WIN32
	if (ok) ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	if (ok) ok = rename(tmp, path) == 0;
#endif
	if (!ok) remove(tmp);
	NVPC_FREE(tmp);
	return ok ? 0 : -1;
}
#endif
