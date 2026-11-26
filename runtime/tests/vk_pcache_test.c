/* vk_pcache_test.c -- the Vulkan pipeline cache file (ANDROID-PERF-03, runtime/platform/sdl-patches/nativ_vk_pcache.h,
 * the file SDL's patched Vulkan backend loads and saves):
 *  - a file written for a device loads back for it, payload unchanged, no .tmp left behind;
 *  - a foreign file (another vendor, GPU, driver version or pipelineCacheUUID), a corrupt one (a flipped payload byte), a
 *    truncated or padded one, garbage, an empty file and a missing one are all refused, so the cache starts empty;
 *  - a payload whose own Vulkan header is bad is refused on load and never written;
 *  - a save that cannot complete (no such folder) fails and leaves the old file as it was.
 * Fail-before: SDL 3.4.16 has no pipeline cache file at all (VK_NULL_HANDLE in vkCreateGraphicsPipelines). Build:
 * cc -O1 -g -Wall vk_pcache_test.c (run_ci_tests.sh). */
#include "../platform/sdl-patches/nativ_vk_pcache.h"
#include <sys/stat.h>
#include <unistd.h>

static int fails;
static void check(const char *what, int ok) { printf("%-80s %s\n", what, ok ? "ok" : "FAILED"); if (!ok) fails++; }

static size_t make_payload(uint8_t *p, size_t n, const NvpcDevice *d) {   /* a driver-like blob: Vulkan header, then data */
	nvpc_wr32(p, 32); nvpc_wr32(p + 4, 1); nvpc_wr32(p + 8, d->vendor); nvpc_wr32(p + 12, d->device); memcpy(p + 16, d->uuid, 16);
	for (size_t i = 32; i < n; i++) p[i] = (uint8_t)(i * 37 + 11);
	return n;
}
static int write_file(const char *path, const void *b, size_t n) { FILE *f = fopen(path, "wb"); if (!f) return -1; size_t w = fwrite(b, 1, n, f); fclose(f); return w == n ? 0 : -1; }
static size_t read_file(const char *path, uint8_t *b, size_t cap) { FILE *f = fopen(path, "rb"); if (!f) return 0; size_t n = fread(b, 1, cap, f); fclose(f); return n; }
static int load_why(const char *path, const NvpcDevice *d) { void *b; size_t o, n; int r = nvpc_load(path, d, &b, &o, &n, 0); if (b) NVPC_FREE(b); return r; }

int main(void) {
	char dir[128], path[192], tmp[200];
	snprintf(dir, sizeof dir, "/tmp/vk_pcache_test_%d", (int)getpid());
	mkdir(dir, 0755);
	snprintf(path, sizeof path, "%s/vulkan_pipelines.bin", dir); snprintf(tmp, sizeof tmp, "%s.tmp", path);
	const NvpcDevice adreno = { 0x5143, 0x6050001, 0x80000000u | 512, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } };
	static uint8_t pl[4096], f[8192], f2[8192];
	size_t n = make_payload(pl, sizeof pl, &adreno);

	check("no file: refused as 'no file'", load_why(path, &adreno) == NVPC_NOFILE);
	check("save: written", nvpc_save(path, &adreno, pl, n) == 0);
	check("save: no .tmp left behind", access(tmp, F_OK) != 0);
	{ void *b; size_t o, pn, fn; int r = nvpc_load(path, &adreno, &b, &o, &pn, &fn);
	  check("load: ok, the payload unchanged, file is 40 + payload bytes", r == NVPC_OK && b && pn == n && fn == n + NVPC_HDR && !memcmp((uint8_t *)b + o, pl, n));
	  if (b) NVPC_FREE(b); }
	size_t fn = read_file(path, f, sizeof f);

	NvpcDevice other = adreno; other.vendor = 0x13b5;   /* Mali */
	check("another GPU vendor: refused", load_why(path, &other) == NVPC_VENDOR);
	other = adreno; other.device = 0x6060001;
	check("another GPU of the vendor: refused", load_why(path, &other) == NVPC_DEVICE);
	other = adreno; other.driver++;
	check("another driver version: refused", load_why(path, &other) == NVPC_DRIVER);
	other = adreno; other.uuid[15] ^= 1;
	check("another pipelineCacheUUID: refused", load_why(path, &other) == NVPC_UUID);

	memcpy(f2, f, fn); f2[NVPC_HDR + 1000] ^= 0x40; write_file(path, f2, fn);
	check("a flipped payload byte: refused (checksum)", load_why(path, &adreno) == NVPC_SUM);
	write_file(path, f, fn - 100);
	check("truncated: refused (size)", load_why(path, &adreno) == NVPC_SIZE);
	memcpy(f2, f, fn); memset(f2 + fn, 0, 16); write_file(path, f2, fn + 16);
	check("padded: refused (size)", load_why(path, &adreno) == NVPC_SIZE);
	write_file(path, f, 20);
	check("shorter than the header: refused", load_why(path, &adreno) == NVPC_SHORT);
	write_file(path, "", 0);
	check("empty file: refused", load_why(path, &adreno) == NVPC_SHORT);
	for (size_t i = 0; i < 300; i++) f2[i] = (uint8_t)(i * 91 + 7);
	write_file(path, f2, 300);
	check("garbage: refused (magic)", load_why(path, &adreno) == NVPC_MAGIC_BAD);
	memcpy(f2, f, fn); nvpc_wr64(f2 + 24, (uint64_t)NVPC_MAX_PAYLOAD + 1); write_file(path, f2, fn);
	check("a payload size beyond the limit: refused", load_why(path, &adreno) == NVPC_TOOBIG);

	uint8_t bad[64]; make_payload(bad, sizeof bad, &adreno); nvpc_wr32(bad + 4, 2);
	uint8_t h[NVPC_HDR]; nvpc_header(h, &adreno, bad, sizeof bad); memcpy(f2, h, NVPC_HDR); memcpy(f2 + NVPC_HDR, bad, sizeof bad);
	write_file(path, f2, NVPC_HDR + sizeof bad);
	check("our header right but the Vulkan header version 2: refused", load_why(path, &adreno) == NVPC_VKHDR);
	make_payload(bad, sizeof bad, &adreno); nvpc_wr32(bad, 100);
	nvpc_header(h, &adreno, bad, sizeof bad); memcpy(f2, h, NVPC_HDR); memcpy(f2 + NVPC_HDR, bad, sizeof bad);
	write_file(path, f2, NVPC_HDR + sizeof bad);
	check("Vulkan headerSize past the payload: refused", load_why(path, &adreno) == NVPC_VKHDR);

	check("a blob with another device's Vulkan header is never written", nvpc_save(path, &other, pl, n) == -1);
	write_file(path, f, fn);
	char nodir[256]; snprintf(nodir, sizeof nodir, "%s/missing/vulkan_pipelines.bin", dir);
	check("a save that cannot be written fails", nvpc_save(nodir, &adreno, pl, n) == -1);
	pl[500] ^= 1; check("a save over an existing file replaces it", nvpc_save(path, &adreno, pl, n) == 0);
	{ void *b; size_t o, pn; int r = nvpc_load(path, &adreno, &b, &o, &pn, 0);
	  check("... and the new payload loads", r == NVPC_OK && pn == n && !memcmp((uint8_t *)b + o, pl, n)); if (b) NVPC_FREE(b); }
	check("reasons have names", !strcmp(nvpc_why(NVPC_SUM), "checksum mismatch (corrupt)") && !strcmp(nvpc_why(99), "?"));

	remove(path); remove(tmp); rmdir(dir);
	printf("%s\n", fails ? "FAILED" : "all passed");
	return fails != 0;
}
