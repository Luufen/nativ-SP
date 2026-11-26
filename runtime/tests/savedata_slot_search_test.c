/* savedata_slot_search_test.c -- MC-01: sceAppUtilSaveDataSlotSearch over apputil.c's slot store (savedata0/.slot<id>).
 *
 * The contract (Vita3K 3ab53c9d SceAppUtil.cpp): cond {type +0 (0 EXIST_SLOT, 1 EMPTY_SLOT), from +4, range +8}; result
 * {hitNum +0, slotList +4}; with a work buffer {buf +0, size +4} slotList is set to buf and filled, ascending by id, with
 * SceAppUtilSaveDataSlot {id, status, userParam, emptyParam 0} (16 bytes) as far as the buffer holds; hitNum counts every
 * hit. A null cond or result, or an unknown type, is SCE_APPUTIL_ERROR_PARAMETER (0x80100001).
 *
 * Fail-before: the name is missing from hle_apputil -- the generic stub answered 0 and left hitNum as the caller's memory
 * held it (Minecraft's offline save scan went back to its menu). -DAPPUTIL_UNDER_TEST='"<older apputil.c>"' compiles the
 * fixture against another copy.
 *   cc -O1 -g -Wall -D_GNU_SOURCE $INC -I $SDL3_INC savedata_slot_search_test.c -o /tmp/savedata_slot_search_test -lpthread \
 *     && /tmp/savedata_slot_search_test
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifndef APPUTIL_UNDER_TEST
#define APPUTIL_UNDER_TEST "../core/apputil.c"
#endif
#include APPUTIL_UNDER_TEST
#include "../core/imedlg.c"   /* (apputil.c's IME dialog shares its state) */

/* ---- apputil.c expects these (dlg_term_test.c's shims) */
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

static int fails;
static void check(const char *what, int ok) { printf("%-66s %s\n", what, ok ? "ok" : "FAILED"); if (!ok) fails++; }
static uint32_t call(const char *name, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
	for (const HleEntry *e = hle_apputil; e->name; e++)
		if (!strcmp(e->name, name)) { Cpu cpu = {0}; cpu.r[0] = a0; cpu.r[1] = a1; cpu.r[2] = a2; cpu.r[3] = a3; cpu.r[13] = 0x82000000u; e->fn(&cpu); return cpu.r[0]; }
	return 0xdeadbeefu;   /* missing: the generic stub would answer 0 and touch nothing */
}
enum { PARAM = 0x80000000u, WORK = 0x80001000u, LIST = 0x80002000u, SCOND = 0x80003000u, RES = 0x80003100u };
static void scond(uint32_t type, uint32_t from, uint32_t range) { memset(G(SCOND), 0, 0x34); WR32(SCOND, type); WR32(SCOND + 4, from); WR32(SCOND + 8, range); }
static void result_garbage(void) { memset(G(RES), 0, 0x28); WR32(RES, 0x7777u); }
static void make_slot(uint32_t id, uint32_t status, uint32_t user) {
	memset(G(PARAM), 0, SLOT_PARAM_SIZE); WR32(PARAM, status); WR32(PARAM + 0x304, user);
	check("  SlotCreate", call("sceAppUtilSaveDataSlotCreate", id, PARAM, 0, 0) == 0);
}

int main(void) {
	recomp_mem = calloc(1, 0x100000000ull);   /* (sparse on Linux) */
	if (!recomp_mem) { perror("calloc"); return 2; }
	char dir[] = "/tmp/slot_search_XXXXXX";
	if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
	setenv("RECOMP_FS", dir, 1);

	scond(0, 0, 256); result_garbage();
	check("no slots, EXIST 0..255 -> 0 and hitNum 0 (was the caller's 0x7777)", call("sceAppUtilSaveDataSlotSearch", 0, SCOND, RES, 0) == 0 && RD32(RES) == 0);

	make_slot(3, 0, 0x11); make_slot(1, 0, 0x22); make_slot(200, 1, 0x33);
	WR32(WORK, LIST); WR32(WORK + 4, 16 * 8); memset(G(LIST), 0xee, 16 * 8);
	scond(0, 0, 256); result_garbage();
	uint32_t r = call("sceAppUtilSaveDataSlotSearch", WORK, SCOND, RES, 0);
	check("slots 1, 3, 200 exist: EXIST 0..255 -> hitNum 3, slotList = work buffer", r == 0 && RD32(RES) == 3 && RD32(RES + 4) == LIST);
	check("list ascending by id: 1, 3, 200", RD32(LIST) == 1 && RD32(LIST + 16) == 3 && RD32(LIST + 32) == 200);
	check("each entry: status and userParam from its param, emptyParam 0", RD32(LIST + 8) == 0x22 && RD32(LIST + 24) == 0x11 &&
	      RD32(LIST + 32 + 4) == 1 && RD32(LIST + 32 + 8) == 0x33 && RD32(LIST + 12) == 0);
	check("nothing written past the hits", RD32(LIST + 48) == 0xeeeeeeeeu);

	/* (ISSUE-21) GetParam reports a slot that is there as AVAILABLE, whatever status the game stored (Vita3K 3ab53c9d);
	 * the stored param is otherwise returned as written. Fail-before: status 1 came back and Terraria deleted its saves. */
	memset(G(PARAM), 0xcc, SLOT_PARAM_SIZE);
	check("SlotGetParam on slot 200 (stored status 1) -> 0", call("sceAppUtilSaveDataSlotGetParam", 200, PARAM, 0, 0) == 0);
	check("  status reads AVAILABLE (0), userParam as stored (0x33)", RD32(PARAM) == 0 && RD32(PARAM + 0x304) == 0x33);
	check("SlotGetParam on a missing slot -> SLOT_NOT_FOUND", call("sceAppUtilSaveDataSlotGetParam", 7, PARAM, 0, 0) == 0x80100641u);

	scond(0, 2, 10); result_garbage();
	check("EXIST 2..11 -> hitNum 1 (slot 3)", call("sceAppUtilSaveDataSlotSearch", WORK, SCOND, RES, 0) == 0 && RD32(RES) == 1 && RD32(LIST) == 3);
	scond(1, 0, 5); result_garbage();
	check("EMPTY 0..4 -> hitNum 3 (0, 2, 4)", call("sceAppUtilSaveDataSlotSearch", WORK, SCOND, RES, 0) == 0 && RD32(RES) == 3 &&
	      RD32(LIST) == 0 && RD32(LIST + 16) == 2 && RD32(LIST + 32) == 4 && RD32(LIST + 8) == 0);
	WR32(WORK + 4, 16 * 1); memset(G(LIST), 0xee, 16 * 8); scond(0, 0, 256); result_garbage();
	check("a 1-entry work buffer: hitNum 3, only the first entry written", call("sceAppUtilSaveDataSlotSearch", WORK, SCOND, RES, 0) == 0 &&
	      RD32(RES) == 3 && RD32(LIST) == 1 && RD32(LIST + 16) == 0xeeeeeeeeu);
	scond(0, 250, 0x10000); result_garbage();
	check("a range past slot 255 stops at 255 (none there)", call("sceAppUtilSaveDataSlotSearch", 0, SCOND, RES, 0) == 0 && RD32(RES) == 0);
	check("null cond -> PARAMETER", call("sceAppUtilSaveDataSlotSearch", WORK, 0, RES, 0) == 0x80100001u);
	scond(5, 0, 1);
	check("unknown search type -> PARAMETER", call("sceAppUtilSaveDataSlotSearch", WORK, SCOND, RES, 0) == 0x80100001u);

	/* (ISSUE-21) sceAppUtilSaveDataDataSave FILE_TRUNCATE (mode 1): written at the offset, then the file ends at offset+size;
	 * the bytes before the offset stay (Vita3K 3ab53c9d). Terraria saves a world as two mode-1 items, 1 MiB at 0 and the
	 * rest at 1 MiB. Fail-before: the second item emptied the file first, leaving zeros before its offset. */
	setenv("RECOMP_SAVE_LATENCY_MS", "0", 1);
	enum { ITEMS = 0x80004000u, NAME = 0x80004100u, BUFA = 0x80004200u, BUFB = 0x80004300u };
	strcpy((char *)G(NAME), "W.WLD"); memset(G(BUFA), 'A', 8); memset(G(BUFB), 'B', 4);
	memset(G(ITEMS), 0, 128);
	WR32(ITEMS, NAME); WR32(ITEMS + 4, BUFA); WR32(ITEMS + 8, 8); WR64(ITEMS + 16, 0); WR32(ITEMS + 24, 1);
	WR32(ITEMS + 64, NAME); WR32(ITEMS + 68, BUFB); WR32(ITEMS + 72, 4); WR64(ITEMS + 80, 8); WR32(ITEMS + 88, 1);
	check("DataSave two mode-1 items (8 B at 0, 4 B at 8) -> 0", call("sceAppUtilSaveDataDataSave", 0, ITEMS, 2, 0) == 0);
	char wp[256]; snprintf(wp, sizeof wp, "%s/savedata0/W.WLD", dir);
	char got[32] = {0}; FILE *wf = fopen(wp, "rb"); size_t gn = wf ? fread(got, 1, sizeof got, wf) : 0; if (wf) fclose(wf);
	check("  the file is AAAAAAAABBBB (12 bytes; the first item kept)", gn == 12 && !memcmp(got, "AAAAAAAABBBB", 12));
	WR32(ITEMS + 8, 2);   /* one mode-1 item, 2 bytes at 0: the file ends at 2 */
	check("DataSave one mode-1 item (2 B at 0) -> 0", call("sceAppUtilSaveDataDataSave", 0, ITEMS, 1, 0) == 0);
	memset(got, 0, sizeof got); wf = fopen(wp, "rb"); gn = wf ? fread(got, 1, sizeof got, wf) : 0; if (wf) fclose(wf);
	check("  the file is AA (truncated to offset + size)", gn == 2 && !memcmp(got, "AA", 2));
	WR32(ITEMS + 64 + 24, 0);   /* mode 0 (FILE): 4 B at 8 written, nothing truncated */
	check("DataSave one mode-0 item (4 B at 8) -> 0", call("sceAppUtilSaveDataDataSave", 0, ITEMS + 64, 1, 0) == 0);
	memset(got, 0xff, sizeof got); wf = fopen(wp, "rb"); gn = wf ? fread(got, 1, sizeof got, wf) : 0; if (wf) fclose(wf);
	check("  the file is AA, 6 zero bytes, BBBB (12 bytes)", gn == 12 && !memcmp(got, "AA\0\0\0\0\0\0BBBB", 12));

	char cmd[128]; snprintf(cmd, sizeof cmd, "rm -rf %s", dir); if (system(cmd)) {}
	printf("%s\n", fails ? "FAILED" : "all passed");
	return fails != 0;
}
