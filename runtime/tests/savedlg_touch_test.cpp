/* savedlg_touch_test.cpp -- SAVEDLG-TOUCH (owner, Galaxy Tab S7, 2026-11-17: "the touch screen save dialog doesn't scale
 * with the resolution (the text); and it can't be operated with the on-screen buttons").
 *
 * Part A, the scale: the dialog (savedlg_ui.cpp, included) sizes its box, rows and buttons in one unit. The overlay's font
 * already follows the window (ui_scale.h: 720 lines = 1, 1600 lines = 2.2); the dialog put a fixed 1 or 2 (window >= 1400
 * wide) on top of that as a window font scale and sized its box by the same 1 or 2, so at 2560x1600 its text was twice the
 * menu's and the box no longer fitted it. Checked at 960x544, 1920x1080 and 2560x1600: the dialog's text is the menu's size
 * and its box keeps the 960x544 proportions (520 px at a 15 px font).
 *
 * Part B, the on-screen buttons: with the dialog up, window.c hands what a finger on them presses to the overlay as SDL
 * gamepad events (osb_pad.h's osb_pad_feed -> the overlay's pad feed, savedlg_ui_gamepad_button/axis). Before, the finger
 * went to the overlay as a touch only and the dialog -- which reads the controller's keys -- saw nothing (case B1). The
 * fingers land where touch_controls.h lays the buttons out in a 2560x1600 window.
 *
 * Fail-before: -DSAVEDLG_UI_UNDER_TEST='"<the pre-fix savedlg_ui.cpp>"' fails the part A checks at 1920x1080 and 2560x1600.
 * Includes apputil.c (the dialog's state machine) like savedlg_pad_test.cpp. Build and run: see README.md here.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifndef APPUTIL_UNDER_TEST
#define APPUTIL_UNDER_TEST "../core/apputil.c"
#endif
#ifndef SAVEDLG_UI_UNDER_TEST
#define SAVEDLG_UI_UNDER_TEST "../ui/savedlg_ui.cpp"
#endif
#include APPUTIL_UNDER_TEST
#include "../core/imedlg.c"
#include SAVEDLG_UI_UNDER_TEST
#include "imgui_internal.h"
#include "../ui/ui_scale.h"
#include "../ui/osb_pad.h"

/* ---- apputil.c expects these (savedlg_pad_test.cpp's shims) ---- */
uint8_t *recomp_mem;
uint32_t recomp_watch_lo = 0, recomp_watch_hi = 0;
void recomp_watch_write(uint32_t a, uint32_t v, int width) { (void)a; (void)v; (void)width; }
uint32_t guest_alloc(uint32_t size, uint32_t align) { (void)align; static uint32_t heap = 0x81000000; uint32_t a = heap; heap += size; return a; }
uint32_t guest_call(Cpu *parent, uint32_t fn, uint32_t a0, uint32_t a1) { (void)parent; (void)fn; (void)a0; (void)a1; return 0; }
int sched_block_begin(void) { return 0; } void sched_block_end(void) {}
void gwatch_read_open(uint32_t addr, uint32_t len) { (void)addr; (void)len; }
int net_online(void) { return 0; }
const char *net_online_needs(void) { return ""; }
int overlay_draws_hud(void) { return 0; }
void overlay_toast_titled(const char *title, const char *body, double seconds) { (void)title; (void)body; (void)seconds; }
void plat_random(void *buf, size_t n) { memset(buf, 0x5a, n); }
const char *settings_str(const char *key, const char *def, char *buf, int bufsize) { (void)key; (void)buf; (void)bufsize; return def; }
void settings_set_str(const char *key, const char *v) { (void)key; (void)v; }
void settings_set_own(const char *key, const char *v) { (void)key; (void)v; }
int settings_save(void) { return 1; }

static int fails, oks;
static void check(const char *what, int ok) {
	printf("%-66s %s\n", what, ok ? "ok" : "FAILED");
	if (!ok) fails++; else oks++;
}

static float g_w = 960, g_h = 544;
static void set_window(float w, float h) {   /* the overlay's own scale for that window (overlay.cpp render_frame, DPI 1) */
	g_w = w; g_h = h;
	ImGui::GetStyle().FontSizeBase = 15.0f * overlay_ui_scale(h, 1.0f);
}
/* (round 2, the owner's check of the first fix: the on-screen buttons still did nothing on the tablet) the frame as
 * overlay.cpp's render_frame runs it on a device with NO controller: ImGui's SDL3 backend clears HasGamepad every frame
 * (its pad list is empty), then render_frame calls savedlg_ui_gamepad_frame(SDL_HasGamepad() || window_osb_shown()) --
 * here 0 || 1, the on-screen buttons showing. -DOSB_KEEPS_GAMEPAD=0: render_frame as it was (SDL_HasGamepad() only):
 * ImGui::NewFrame wipes every gamepad key the on-screen buttons pressed (imgui.cpp "Clear gamepad data if disabled"). */
#ifndef OSB_KEEPS_GAMEPAD
#define OSB_KEEPS_GAMEPAD 1
#endif
static void frame(void) {
	ImGuiIO &io = ImGui::GetIO();
	io.DisplaySize = ImVec2(g_w, g_h);
	io.DeltaTime = 1.0f / 60.0f;
	io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;   /* ImGui_ImplSDL3_NewFrame -> UpdateGamepads, no pad */
	savedlg_ui_gamepad_frame(0 /* SDL_HasGamepad() */ || OSB_KEEPS_GAMEPAD /* window_osb_shown() */);
	ImGui::NewFrame();
	savedlg_ui_build(g_w, g_h);
	ImGui::Render();
}

/* a dialog step as apputil.c's sd_step leaves it (savedlg_demo: three rows; a question) */
static void open_step(int kind) { savedlg_demo(kind); frame(); }   /* (one frame: the step arms, nothing held) */

/* ---- part B: fingers on the on-screen buttons of a 2560x1600 window, through window.c's feed (osb_pad.h) ---- */
static OsbLayout L; static OsbPadFed fed;
static void send_button(int b, int down) { savedlg_ui_gamepad_button(b, down); }
static void send_axis(int a, int v) { savedlg_ui_gamepad_axis(a, v); }
/* a finger held on control c at (dx, dy) units from its centre for a frame, then lifted for a frame; feed 0: the host as it
 * was (the touch reaches the overlay as a touch only, nothing is fed) */
static void tap(int c, float dx, float dy, int feed_on) {
	const OsbCtrl *k = &L.c[c];
	const float x = k->x + k->w * 0.5f + dx * L.unit, y = k->y + k->h * 0.5f + dy * L.unit;
	const int hit = osb_hit(&L, x, y);
	unsigned bits = 0, sx = 128, sy = 128; int stick = 0;
	if (hit == OSB_LSTICK) { unsigned char a, b; osb_stick(k, x, y, &a, &b); sx = a; sy = b; stick = 1; }
	else bits = osb_bits(&L, hit, x, y);
	if (feed_on) osb_pad_feed(&fed, bits, stick, sx, sy, send_button, send_axis);
	frame();
	if (feed_on) osb_pad_feed(&fed, 0, 0, 128, 128, send_button, send_axis);
	frame();
}

int main(void) {
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;
	unsigned char *px; int fw, fh;
	io.Fonts->GetTexDataAsRGBA32(&px, &fw, &fh);

	/* part A: the text is the menu's size and the box keeps its proportions at every window size */
	static const float sizes[3][2] = {{960, 544}, {1920, 1080}, {2560, 1600}};
	for (int i = 0; i < 3; i++) {
		set_window(sizes[i][0], sizes[i][1]);
		open_step(SAVEDLG_LIST); frame();
		ImGuiWindow *win = ImGui::FindWindowByName("##savedlg");
		/* (the font size as ImGui uses it: rounded to a whole pixel, imgui.cpp UpdateCurrentFontSize) */
		const float menu_font = floorf(ImGui::GetStyle().FontSizeBase + 0.5f);
		const float font = win ? floorf(ImGui::GetStyle().FontSizeBase * win->FontWindowScale + 0.5f) : 0, ratio = win && font > 0 ? win->Size.x / font : 0;
		char what[96];
		snprintf(what, sizeof what, "%.0fx%.0f: the dialog's text is the menu's (%.1f px; menu %.1f)", sizes[i][0], sizes[i][1], font, menu_font);
		check(what, win && fabsf(font - menu_font) < 0.01f);
		snprintf(what, sizeof what, "%.0fx%.0f: box width %.0f = 34.7 em (%.1f em)", sizes[i][0], sizes[i][1], win ? win->Size.x : 0, ratio);
		check(what, win && fabsf(ratio - 520.0f / 15.0f) < 0.5f);
		snprintf(what, sizeof what, "%.0fx%.0f: the box fits the window", sizes[i][0], sizes[i][1]);
		check(what, win && win->Size.x <= sizes[i][0] && win->Size.y <= sizes[i][1]);
		savedlg_demo(SAVEDLG_NONE); frame();
	}

	/* part B at 2560x1600 */
	set_window(2560, 1600);
	osb_layout(&L, 2560, 1600, 1.0f, 0);
	osb_pad_reset(&fed);

	/* B1 (the reported failure, the host as it was): a finger on the on-screen Cross answers nothing */
	open_step(SAVEDLG_LIST);
	tap(OSB_CROSS, 0, 0, 0);
	check("B1 before: on-screen Cross, not fed -> the list stays unanswered", sd.sub == ST_RUNNING && sd.kind == SAVEDLG_LIST);
	savedlg_demo(SAVEDLG_NONE); frame();

	/* B2: Cross chooses the focused row (row 0, slot 0) */
	open_step(SAVEDLG_LIST);
	tap(OSB_CROSS, 0, 0, 1);
	check("B2 on-screen Cross chooses the focused row (slot 0)", sd.sub == ST_FINISHED && sd.result == 0 && sd.slot == 0);

	/* B3: the D-pad's lower arm moves down, twice, then Cross: row 2 */
	open_step(SAVEDLG_LIST);
	tap(OSB_DPAD, 0, 1.1f, 1); tap(OSB_DPAD, 0, 1.1f, 1);
	tap(OSB_CROSS, 0, 0, 1);
	check("B3 on-screen D-pad down x2, Cross -> row 2 (slot 2)", sd.sub == ST_FINISHED && sd.result == 0 && sd.slot == 2);

	/* B4: the left stick pushed down moves too */
	open_step(SAVEDLG_LIST);
	tap(OSB_LSTICK, 0, 1.4f, 1);
	tap(OSB_CROSS, 0, 0, 1);
	check("B4 on-screen left stick down, Cross -> row 1 (slot 1)", sd.sub == ST_FINISHED && sd.result == 0 && sd.slot == 1);

	/* B5: Circle goes back (canceled) */
	open_step(SAVEDLG_LIST);
	tap(OSB_CIRCLE, 0, 0, 1);
	check("B5 on-screen Circle backs out of the list", sd.sub == ST_FINISHED && sd.result == 1);

	/* B6: a question: Cross answers Yes; the D-pad's right arm then Cross answers No; Circle backs out */
	open_step(SAVEDLG_YESNO);
	tap(OSB_CROSS, 0, 0, 1);
	check("B6 question: on-screen Cross -> Yes", sd.sub == ST_FINISHED && sd.result == 0 && sd.button == SAVEDLG_YES);
	open_step(SAVEDLG_YESNO);
	tap(OSB_DPAD, 1.1f, 0, 1);
	tap(OSB_CROSS, 0, 0, 1);
	check("B7 question: on-screen D-pad right, Cross -> No", sd.sub == ST_FINISHED && sd.result == 0 && sd.button == SAVEDLG_NO);
	open_step(SAVEDLG_YESNO);
	tap(OSB_CIRCLE, 0, 0, 1);
	check("B8 question: on-screen Circle backs out", sd.sub == ST_FINISHED && sd.result == 1);

	/* B9: the feed hands each change over once (a held button is one press, its release one release) */
	{	static int n_down, n_up; n_down = n_up = 0; OsbPadFed f; osb_pad_reset(&f);
		auto b = [](int, int down) { if (down) n_down++; else n_up++; }; auto a = [](int, int) {};
		osb_pad_feed(&f, OSB_BIT_CROSS, 0, 128, 128, b, a); osb_pad_feed(&f, OSB_BIT_CROSS, 0, 128, 128, b, a); osb_pad_feed(&f, 0, 0, 128, 128, b, a);
		check("B9 a held button: one down, one up", n_down == 1 && n_up == 1);
	}

	ImGui::DestroyContext();
	if (fails) { printf("%d checks FAILED (%d ok)\n", fails, oks); return 1; }
	printf("all passed (%d ok)\n", oks);
	return 0;
}
