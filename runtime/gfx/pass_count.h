/* ANDROID-PERF-04: RECOMP_GPU_PASS_COUNT=1 -- how often an open scene's render pass is broken and begun again, and why. On a
 * tile-based GPU (Adreno, Mali) each break stores the colour and depth attachments and loads them again. Pure counting, no SDL
 * (runtime/tests/pass_count_test.c). The calls, all on the thread that records the scene:
 *   pc_scene_begin   a scene starts: its first render pass breaks nothing
 *   pc_flush         a scene command buffer is submitted for `why`; with draws in it, a later pass of the same scene is a break
 *   pc_scene_pass    a render pass of the scene's draws begins; `inside`: what ended the previous one within the same command
 *                    buffer (a framebuffer copy, a view pass), 0 for the command buffer's first pass (then the last flush's why)
 *   pc_other_pass    any other render pass (view, transfer, motion)
 *   pc_upload        a texture upload while draws were pending that did not flush them (its own command buffer, ahead of them) */
#ifndef PASS_COUNT_H
#define PASS_COUNT_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { PC_UPLOAD, PC_READBACK, PC_OTHER, PC_KINDS };
#define PC_WHYS 24
typedef struct {
	uint32_t frames, passes, splits[PC_KINDS], uploads_unbroken, scene_passes;
	const char *pending;   /* the last flush of this scene that held draws */
	struct { const char *why; uint32_t n; } why[PC_WHYS]; int nwhy;
} PassCount;

static int pc_kind(const char *why) {
	if (!strcmp(why, "texture re-upload") || !strcmp(why, "texture released") || !strcmp(why, "target upload")) return PC_UPLOAD;
	if (!strcmp(why, "target download") || !strcmp(why, "sync to guest")) return PC_READBACK;
	return PC_OTHER;
}
static void pc_scene_begin(PassCount *c) { c->scene_passes = 0; c->pending = 0; }
static void pc_flush(PassCount *c, const char *why, int ndraws) { if (ndraws) c->pending = why; }
static void pc_scene_pass(PassCount *c, const char *inside) {
	c->passes++;
	const char *why = inside ? inside : c->pending;
	c->pending = 0;
	if (!c->scene_passes++) return;
	if (!why) why = "unknown";
	c->splits[pc_kind(why)]++;
	int i; for (i = 0; i < c->nwhy; i++) if (!strcmp(c->why[i].why, why)) break;
	if (i == c->nwhy) { if (c->nwhy == PC_WHYS) return; c->why[c->nwhy].why = why; c->why[c->nwhy++].n = 0; }
	c->why[i].n++;
}
static void pc_other_pass(PassCount *c) { c->passes++; }
static void pc_upload(PassCount *c) { c->uploads_unbroken++; }
/* one frame done; every `every` frames the totals' per-frame averages into out (returns 1), then they start again */
static int pc_frame(PassCount *c, uint32_t every, char *out, size_t n) {
	if (++c->frames < every) return 0;
	const double f = c->frames;
	int k = snprintf(out, n, "gpu passes: %u frames, per frame: %.2f render passes begun; scene broken by upload %.2f, readback %.2f, other %.2f; uploads that broke nothing %.2f; breaks by reason:",
		c->frames, c->passes / f, c->splits[PC_UPLOAD] / f, c->splits[PC_READBACK] / f, c->splits[PC_OTHER] / f, c->uploads_unbroken / f);
	for (int i = 0; i < c->nwhy && k > 0 && (size_t)k < n; i++) k += snprintf(out + k, n - (size_t)k, "%s %s %.2f", i ? "," : "", c->why[i].why, c->why[i].n / f);
	if (!c->nwhy && k > 0 && (size_t)k < n) snprintf(out + k, n - (size_t)k, " none");
	c->frames = c->passes = c->uploads_unbroken = 0; memset(c->splits, 0, sizeof c->splits); c->nwhy = 0;
	return 1;
}
#endif
