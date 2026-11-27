/* tex_caps.h (ANDROID-PERF-06): which GPUs get the texture-pack limits, the RAM budget, the overrides and the sizes.
 *     cc -O1 -g -Wall -I ../gfx tex_caps_test.c -o /tmp/tex_caps_test && /tmp/tex_caps_test */
#include <stdio.h>
#include "tex_caps.h"

static int checks, failed;
#define CHECK(x) do { checks++; if (!(x)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
#define PLAN(android, vendor, ram) tex_caps_plan(android, vendor, ram, 0, 0, 0, 0)

int main(void) {
	TexCaps c;
	/* Android: Mali and PowerVR are limited; Adreno, Xclipse and an unknown GPU aren't */
	c = PLAN(1, 0x13B5, 7600); CHECK(c.max_side == 2048); CHECK(c.budget_mb == 1024);
	c = PLAN(1, 0x1010, 7600); CHECK(c.max_side == 2048); CHECK(c.budget_mb == 1024);
	c = PLAN(1, 0x5143, 7600); CHECK(!c.max_side); CHECK(!c.budget_mb);
	c = PLAN(1, 0x144D, 7600); CHECK(!c.max_side); CHECK(!c.budget_mb);
	c = PLAN(1, 0, 7600); CHECK(!c.max_side); CHECK(!c.budget_mb);
	/* desktop with the same vendors: unchanged */
	c = PLAN(0, 0x13B5, 32000); CHECK(!c.max_side); CHECK(!c.budget_mb);
	c = PLAN(0, 0x1010, 32000); CHECK(!c.max_side); CHECK(!c.budget_mb);

	/* the budget from RAM */
	CHECK(tex_caps_ram_budget_mb(0) == 1024);
	CHECK(tex_caps_ram_budget_mb(800) == 256);
	CHECK(tex_caps_ram_budget_mb(2800) == 700);
	CHECK(tex_caps_ram_budget_mb(3700) == 925);
	CHECK(tex_caps_ram_budget_mb(5600) == 1024);
	CHECK(tex_caps_ram_budget_mb(8192) == 1024);
	CHECK(tex_caps_ram_budget_mb(11500) == 1536);

	/* overrides, on a limited and an unlimited device; the environment wins over the setting */
	c = tex_caps_plan(1, 0x13B5, 7600, "4096", "1024", "2000", "500"); CHECK(c.max_side == 4096); CHECK(c.budget_mb == 2000);
	c = tex_caps_plan(1, 0x13B5, 7600, 0, "1024", "", "500"); CHECK(c.max_side == 1024); CHECK(c.budget_mb == 500);
	c = tex_caps_plan(1, 0x13B5, 7600, "0", "1024", "0", 0); CHECK(!c.max_side); CHECK(!c.budget_mb);
	c = tex_caps_plan(1, 0x13B5, 7600, 0, "0", 0, "0"); CHECK(!c.max_side); CHECK(!c.budget_mb);
	c = tex_caps_plan(1, 0x5143, 7600, "2048", 0, 0, "300"); CHECK(c.max_side == 2048); CHECK(c.budget_mb == 300);
	c = tex_caps_plan(0, 0x10DE, 32000, 0, "512", 0, 0); CHECK(c.max_side == 512); CHECK(!c.budget_mb);
	/* not a whole number >= 0: unset */
	c = tex_caps_plan(1, 0x13B5, 7600, "-1", "abc", "2k", " 5"); CHECK(c.max_side == 2048); CHECK(c.budget_mb == 1024);

	/* DDS: mip levels left out */
	CHECK(tex_caps_skip_levels(4096, 4096, 13, 2048) == 1);
	CHECK(tex_caps_skip_levels(8192, 1024, 14, 2048) == 2);
	CHECK(tex_caps_skip_levels(2048, 2048, 1, 2048) == 0);
	CHECK(tex_caps_skip_levels(4096, 4096, 1, 2048) == -1);
	CHECK(tex_caps_skip_levels(4096, 4096, 1, 0) == 0);

	/* PNG: downscale steps */
	uint32_t w = 0, h = 0;
	CHECK(!tex_caps_downscale_step(2048, 1024, 2048, &w, &h));
	CHECK(!tex_caps_downscale_step(8192, 8192, 0, &w, &h));
	CHECK(tex_caps_downscale_step(8192, 4096, 2048, &w, &h)); CHECK(w == 4096 && h == 2048);
	CHECK(tex_caps_downscale_step(4096, 2048, 2048, &w, &h)); CHECK(w == 2048 && h == 1024);
	CHECK(tex_caps_downscale_step(3000, 1500, 2048, &w, &h)); CHECK(w == 2048 && h == 1024);
	CHECK(tex_caps_downscale_step(16384, 2, 2048, &w, &h)); CHECK(w == 8192 && h == 1);

	/* the budget itself */
	CHECK(!tex_caps_over(1023u << 20, 1u << 20, 1024));
	CHECK(tex_caps_over(1023u << 20, (1u << 20) + 1, 1024));
	CHECK(!tex_caps_over(4000ull << 20, 4000ull << 20, 0));
	CHECK(tex_caps_over(1500ull << 20, 4000ull << 20, 1536));   /* past 32 bits */

	printf("tex_caps_test: %d checks, %d failed\n", checks, failed);
	return failed != 0;
}
