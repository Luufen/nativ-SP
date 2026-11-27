/*
 * tex_caps.h: the memory limits of texture-pack replacements (texpack.c), ANDROID-PERF-06. A pack loaded at its own size
 * has no memory bound of its own (the texture cache charges an entry by the game texture's size), and Arm Mali and
 * Imagination PowerVR phone GPUs have less memory bandwidth. So on Android on those two vendors a replacement's longest
 * side is capped and the replacements together get a budget from the device's RAM. Everywhere else (desktop, Adreno,
 * Xclipse, an unknown GPU) there is no limit. On any platform either limit can be set, 0 for none:
 *   RECOMP_TEXPACK_MAX_SIZE  / setting texpack_max_size    longest side in pixels
 *   RECOMP_TEXPACK_BUDGET_MB / setting texpack_budget_mb   MiB of replacement textures
 * The environment variable wins over the setting; anything but a whole number >= 0 counts as unset.
 *
 * Pure: no Vulkan or SDL headers (tests/tex_caps_test.c).
 */
#ifndef NATIV_TEX_CAPS_H
#define NATIV_TEX_CAPS_H
#include <stdint.h>
#include <stdlib.h>

#define TEXCAPS_VENDOR_ARM 0x13B5u      /* VkPhysicalDeviceProperties.vendorID (PanVK reports it too) */
#define TEXCAPS_VENDOR_IMGTEC 0x1010u
#define TEXCAPS_DEFAULT_MAX_SIZE 2048u

typedef struct {
	uint32_t max_side;     /* 0: none */
	uint32_t budget_mb;    /* 0: none */
} TexCaps;

/* the default budget: 1 GiB, 1.5 GiB above 8 GiB of RAM, never more than a quarter of it (but at least 256 MiB);
 * ram_mb is what the OS reports (below the marketed size), 0 unknown. Chosen, not measured on a device. */
static inline uint32_t tex_caps_ram_budget_mb(uint32_t ram_mb) {
	if (!ram_mb) return 1024;
	uint32_t b = ram_mb > 8192 ? 1536 : 1024;
	if (b > ram_mb / 4) b = ram_mb / 4;
	return b < 256 ? 256 : b;
}

static inline int tex_caps_number(const char *s, uint32_t *out) {
	if (!s || *s < '0' || *s > '9') return 0;
	char *end; unsigned long v = strtoul(s, &end, 10);
	if (*end || v > 1000000ul) return 0;
	*out = (uint32_t)v; return 1;
}

/* env_* and set_*: the environment variable's and the setting's text, NULL or "" unset */
static inline TexCaps tex_caps_plan(int android, uint32_t vendor_id, uint32_t ram_mb, const char *env_size, const char *set_size,
                                    const char *env_budget, const char *set_budget) {
	const int limited = android && (vendor_id == TEXCAPS_VENDOR_ARM || vendor_id == TEXCAPS_VENDOR_IMGTEC);
	TexCaps c = { limited ? TEXCAPS_DEFAULT_MAX_SIZE : 0, limited ? tex_caps_ram_budget_mb(ram_mb) : 0 };
	if (!tex_caps_number(env_size, &c.max_side)) tex_caps_number(set_size, &c.max_side);
	if (!tex_caps_number(env_budget, &c.budget_mb)) tex_caps_number(set_budget, &c.budget_mb);
	return c;
}

/* how many leading mip levels of a w x h chain of `levels` to leave out so the longest side fits; -1: none is small enough */
static inline int tex_caps_skip_levels(uint32_t w, uint32_t h, uint32_t levels, uint32_t max_side) {
	for (uint32_t l = 0; l < levels; l++, w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1)
		if (!max_side || (w <= max_side && h <= max_side)) return (int)l;
	return -1;
}

/* the next size on the way down for an image without mips (0: it fits): halved while that stays at or above the cap (a
 * linear halving averages every texel), then the last step to the cap, the shape kept */
static inline int tex_caps_downscale_step(uint32_t w, uint32_t h, uint32_t max_side, uint32_t *nw, uint32_t *nh) {
	const uint32_t side = w > h ? w : h;
	if (!max_side || side <= max_side) return 0;
	const uint32_t to = side / 2 >= max_side ? side / 2 : max_side;
	*nw = (uint32_t)((uint64_t)w * to / side); *nh = (uint32_t)((uint64_t)h * to / side);
	if (!*nw) *nw = 1;
	if (!*nh) *nh = 1;
	return 1;
}

/* would `add` more bytes pass the budget? */
static inline int tex_caps_over(uint64_t used, uint64_t add, uint32_t budget_mb) {
	return budget_mb && used + add > (uint64_t)budget_mb << 20;
}
#endif
