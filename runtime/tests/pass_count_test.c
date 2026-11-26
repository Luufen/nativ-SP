/* ANDROID-PERF-04 (pass_count.h): which render passes count as a broken scene, and under which reason.
 * cc -O1 -g -Wall -I ../gfx pass_count_test.c -o pass_count_test && ./pass_count_test */
#include <stdio.h>
#include <string.h>
#include "pass_count.h"

static int fails, checks;
#define CHECK(x) do { checks++; if (!(x)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

int main(void) {
	PassCount c; memset(&c, 0, sizeof c); char out[1024];
	/* scene 1: one pass, ended -- nothing broken; an upload ahead of it neither */
	pc_scene_begin(&c); pc_upload(&c); pc_scene_pass(&c, 0); pc_flush(&c, "end of scene", 3);
	CHECK(c.passes == 1 && c.splits[PC_UPLOAD] == 0 && c.splits[PC_OTHER] == 0);
	/* scene 2: a new scene's first pass is no break, whatever the last flush was */
	pc_scene_begin(&c); pc_scene_pass(&c, 0);
	/* a framebuffer copy inside the command buffer */
	pc_scene_pass(&c, "fragcolor copy");
	CHECK(c.splits[PC_OTHER] == 1);
	/* a re-upload of a texture the pending draws sample: flush, then the scene goes on */
	pc_flush(&c, "texture re-upload", 5); pc_scene_pass(&c, 0);
	CHECK(c.splits[PC_UPLOAD] == 1);
	/* a download of the scene's target */
	pc_flush(&c, "target download", 2); pc_scene_pass(&c, 0);
	CHECK(c.splits[PC_READBACK] == 1);
	/* a flush without draws (views alone) keeps the earlier cause; a flush with nothing after it counts nothing */
	pc_flush(&c, "target upload", 4); pc_flush(&c, "present", 0); pc_scene_pass(&c, 0);
	CHECK(c.splits[PC_UPLOAD] == 2);
	pc_flush(&c, "texture released", 4);
	pc_scene_begin(&c); pc_scene_pass(&c, 0);
	CHECK(c.splits[PC_UPLOAD] == 2 && c.splits[PC_OTHER] == 1);
	pc_other_pass(&c);
	CHECK(c.passes == 8 && c.uploads_unbroken == 1);
	CHECK(!pc_frame(&c, 2, out, sizeof out));
	CHECK(pc_frame(&c, 2, out, sizeof out));
	CHECK(strstr(out, "4.00 render passes begun") && strstr(out, "by upload 1.00, readback 0.50, other 0.50") && strstr(out, "uploads that broke nothing 0.50"));
	CHECK(strstr(out, "texture re-upload 0.50") && strstr(out, "target upload 0.50") && strstr(out, "fragcolor copy 0.50"));
	CHECK(c.frames == 0 && c.passes == 0 && c.nwhy == 0);
	pc_frame(&c, 1, out, sizeof out);
	CHECK(strstr(out, "breaks by reason: none"));
	printf("pass_count_test: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
