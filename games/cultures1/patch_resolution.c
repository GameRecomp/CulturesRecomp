/* SPDX-License-Identifier: MIT */
/*
 * Cultures 1's resolution: the game's two lists of resolutions (Graphic Options in the main menu,
 * "Screen resolutions" in a mission) offer "Window x1" to "Window x3" first, the size of the
 * window divided by that factor, and only 16-bit modes (hle/ddraw lists no 8-bit ones). The choice
 * is settings.ini's `resolution` (rc_plat_game_size) both ways: it is what the lists show chosen,
 * and choosing in a list stores it. While a mission plays at a window size, the game changes its
 * mode when the window does. The menus keep their 800x600 (640x480 for the intro).
 *
 * In the game's terms a window entry is a mode of width 0 and height 1 to 5 (the factor), 16 bits:
 *
 * - the mode table (0xb8 into the application object: the count at +0xc, 12-byte entries of width,
 *   height and depth from +0x10), filled once from EnumDisplayModes, gets the five entries first;
 * - the settings (read from the registry into an object: the width at +0x11, the height at +0x13,
 *   the depth at +0x15, 16-bit) get the choice of settings.ini before the game checks them, and a
 *   window entry passes that check;
 * - every mode change goes through one function (application object; width, height, depth, a flag:
 *   1 skips the check against the table and the desktop): at its start a window entry becomes the
 *   window's size, and 8 bits become 16;
 * - the places that write an entry's label ("%d x %d ... %d", "%d * %d pixel - %d bit") get
 *   "Window x1.5" written over theirs;
 * - in a mission (the dword at `mission` is 1) the main loop, between messages and the frame, calls
 *   that function when the window's size has stayed different for a moment.
 *
 * docs/RESEARCH.md, "Resolutions".
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "patches.h"
#include "rc/cpu.h"
#include "rc/dispatch.h"
#include "rc/game.h"
#include "rc/hle.h"
#include "rc/hle_modules.h"
#include "rc/log.h"
#include "rc/mem.h"
#include "rc/platform.h"
#include "rc/process.h"

typedef struct build {
    uint32_t settings_read; /* esi: the settings object */
    uint32_t checked;       /* after the game checked them against the mode table: al */
    uint32_t change;        /* the mode change function (thiscall, 4 arguments) */
    uint32_t enumerated;    /* after EnumDisplayModes: edi is the mode table */
    uint32_t label_mission; /* after the label's sprintf: [ebp-0x1c] is the entry's height */
    uint32_t label_menu;    /* after the label's sprintf: edi+0x10 is the entry */
    uint32_t label_screen;  /* the same in "Screen resolutions" (its sprintf's arguments still on the stack) */
    uint32_t loop;          /* the main loop, after the messages */
    uint32_t click;         /* the mission list's call of the mode change */
    uint32_t start;         /* a mission starts: the settings object is read next */
    uint32_t settings;      /* the settings object */
    uint32_t app;           /* the application object */
    uint32_t mission;       /* 1 while a mission plays */
} build;

static const build builds[] = {
    {0x00466c24, 0x00466c3d, 0x00462f21, 0x00493597, 0x00464522, 0x0046d937, 0x0043d378, 0x00497e4b, 0x004643e8, 0x00451fed, 0x004bcb88, 0x004bd878,
     0x004be1b0}, /* GOG EN, DE */
    {0x00467712, 0x0046772b, 0x004639ff, 0x004944f7, 0x00464fe8, 0x0046e7bd, 0x0043db2b, 0x00498dab, 0x00464eae, 0x004527bd, 0x004bdef8, 0x004bebe8,
     0x004bf528}, /* Gold DE */
};
#define NBUILDS (sizeof builds / sizeof builds[0])

/* The window factors, in tenths, by the entry's height (1 to 5). */
static const int factors[] = {10, 15, 20, 25, 30};
#define NFACTORS (sizeof factors / sizeof factors[0])

#define TABLE_MAX 0x50 /* entries the game's table holds */
#define SETTLE_MS 300  /* how long the window keeps a new size before the mode follows */

static const build *find(uint32_t eip, size_t field)
{
    for (size_t i = 0; i < NBUILDS; i++) {
        if (*(const uint32_t *)((const char *)&builds[i] + field) == eip) {
            return &builds[i];
        }
    }
    return NULL;
}
#define FIND(c, f) find((c)->eip, offsetof(build, f))

/* The window entry (1 to 5) settings.ini chooses, or 0. */
static int window_entry(void)
{
    rc_display d = rc_plat_display_get();
    if (d.game_w != -1) {
        return 0;
    }
    int best = 1;
    for (size_t k = 0; k < NFACTORS; k++) {
        int s = d.game_scale >= 10 ? d.game_scale : 10;
        int diff = s - factors[k], bdiff = s - factors[best - 1];
        if ((diff < 0 ? -diff : diff) < (bdiff < 0 ? -bdiff : bdiff)) {
            best = (int)k + 1;
        }
    }
    return best;
}

/* The size of window entry `k`. */
static bool entry_size(int k, int *w, int *h)
{
    rc_display d = rc_plat_display_get();
    rc_display was = d;
    d.game_w = d.game_h = -1;
    d.game_scale = factors[k - 1];
    rc_plat_display(&d);
    bool ok = rc_plat_game_size(w, h);
    rc_plat_display(&was);
    return ok;
}

/* An entry's label, in the language of the install. */
static void window_label(char *out, size_t cap, uint32_t k)
{
    const char *lang = rc_game_language();
    bool german = lang && !strcmp(lang, "Deutsch");
    int f = factors[k - 1];
    if (f % 10) {
        snprintf(out, cap, "%s x%d%c%d", german ? "Fenster" : "Window", f / 10, german ? ',' : '.', f % 10);
    } else {
        snprintf(out, cap, "%s x%d", german ? "Fenster" : "Window", f / 10);
    }
}

/* Store a choice of the game's (a window entry, or a size) as settings.ini's resolution. */
static void choose(uint32_t w, uint32_t h)
{
    rc_display d = rc_plat_display_get();
    if (w == 0 && h >= 1 && h <= NFACTORS) {
        if (d.game_w == -1 && d.game_scale == factors[h - 1]) {
            return;
        }
        d.game_w = d.game_h = -1;
        d.game_scale = factors[h - 1];
    } else if (w && h) {
        if (d.game_w == (int)w && d.game_h == (int)h) {
            return;
        }
        d.game_w = (int)w;
        d.game_h = (int)h;
    } else {
        return;
    }
    rc_plat_display(&d);
    rc_plat_display_store();
}

/* What the settings object held when this patch last wrote it (or saw a choice stored): a
 * different value there is a choice made in the main menu's list. */
static uint32_t known_w = UINT32_MAX, known_h = UINT32_MAX;

static void put_settings(uint32_t s);

/* The settings object, if the game never read the registry into it (the Gold CD's installer writes
 * no settings, and the game keeps its defaults): settings.ini's choice, once. */
static void init_settings(const build *b)
{
    if (known_w == UINT32_MAX) {
        put_settings(b->settings);
    }
}

static void sync_settings(const build *b)
{
    uint32_t w = rc_ld16(b->settings + 0x11), h = rc_ld16(b->settings + 0x13);
    if (w != known_w || h != known_h) {
        if (known_w != UINT32_MAX) {
            choose(w, h);
        }
        known_w = w;
        known_h = h;
    }
}

/* settings.ini's choice into the settings object at `s`. */
static void put_settings(uint32_t s)
{
    rc_display d = rc_plat_display_get();
    int k = window_entry(), w, h;
    if (k) {
        rc_st16(s + 0x11, 0);
        rc_st16(s + 0x13, (uint16_t)k);
    } else if (d.game_w != 0 && rc_plat_game_size(&w, &h)) {
        rc_st16(s + 0x11, (uint16_t)w);
        rc_st16(s + 0x13, (uint16_t)h);
    }
    if (d.game_w != 0 || rc_ld16(s + 0x15) == 8) {
        rc_st16(s + 0x15, 16);
    }
    known_w = rc_ld16(s + 0x11);
    known_h = rc_ld16(s + 0x13);
}

static void settings_read(rc_cpu *c) { put_settings(c->r[RC_ESI]); }

/* The game's check of the settings (in the table, and in the desktop's depth unless 8 bits, no
 * larger than the desktop): a window entry passes, and so does any 16-bit size (the lists have
 * nothing else now; DirectDraw sets any), rather than giving way to 800x600. */
static void checked(rc_cpu *c)
{
    const build *b = FIND(c, checked);
    if (!b) {
        return;
    }
    uint32_t w = rc_ld16(b->settings + 0x11), h = rc_ld16(b->settings + 0x13), bpp = rc_ld16(b->settings + 0x15);
    if ((w == 0 && h >= 1 && h <= NFACTORS) || (bpp == 16 && w >= 640 && h >= 480)) {
        c->r[RC_EAX] = (c->r[RC_EAX] & ~0xffu) | 1;
    }
}

/* The mode table: the window entries first, and a size of settings.ini's that the list lacks. */
static void enumerated(rc_cpu *c)
{
    uint32_t t = c->r[RC_EDI];
    uint32_t n = rc_ld32(t + 0xc);
    if (n + NFACTORS + 1 > TABLE_MAX) {
        n = TABLE_MAX - NFACTORS - 1;
    }
    memmove(rc_ptr(t + 0x10 + 12 * NFACTORS), rc_ptr(t + 0x10), 12 * n);
    for (uint32_t k = 0; k < NFACTORS; k++) {
        rc_st32(t + 0x10 + 12 * k, 0);
        rc_st32(t + 0x14 + 12 * k, k + 1);
        rc_st32(t + 0x18 + 12 * k, 16);
    }
    n += NFACTORS;
    rc_display d = rc_plat_display_get();
    int w, h;
    if (d.game_w > 0 && rc_plat_game_size(&w, &h)) {
        bool listed = false;
        for (uint32_t i = 0; i < n; i++) {
            listed |= rc_ld32(t + 0x10 + 12 * i) == (uint32_t)w && rc_ld32(t + 0x14 + 12 * i) == (uint32_t)h;
        }
        if (!listed) {
            rc_st32(t + 0x10 + 12 * n, (uint32_t)w);
            rc_st32(t + 0x14 + 12 * n, (uint32_t)h);
            rc_st32(t + 0x18 + 12 * n, 16);
            n++;
        }
    }
    rc_st32(t + 0xc, n);
}

/* The start of the mode change: [esp+4] width, +8 height, +0xc depth, +0x10 flag. */
static void change(rc_cpu *c)
{
    uint32_t sp = c->r[RC_ESP];
    uint32_t w = rc_ld32(sp + 4), h = rc_ld32(sp + 8);
    int ww, wh;
    if (w == 0 && h >= 1 && h <= NFACTORS && entry_size((int)h, &ww, &wh)) {
        rc_st32(sp + 4, (uint32_t)ww);
        rc_st32(sp + 8, (uint32_t)wh);
        rc_st32(sp + 0x10, 1); /* not in the table, perhaps larger than the desktop the game knew */
        char name[32];
        window_label(name, sizeof name, h);
        rc_log("resolution: %dx%d (%s)", ww, wh, name);
    }
    if (rc_ld32(sp + 0xc) == 8) {
        rc_st32(sp + 0xc, 16);
    }
}

/* The mission list's call of the mode change (the entry's width, height and depth on the stack):
 * the choice is settings.ini's from now on, and the settings object's. */
static void click(rc_cpu *c)
{
    const build *b = FIND(c, click);
    uint32_t sp = c->r[RC_ESP];
    uint32_t w = rc_ld32(sp), h = rc_ld32(sp + 4), bpp = rc_ld32(sp + 8);
    choose(w, h);
    if (b) {
        rc_st16(b->settings + 0x11, (uint16_t)w);
        rc_st16(b->settings + 0x13, (uint16_t)h);
        rc_st16(b->settings + 0x15, (uint16_t)bpp);
        known_w = w;
        known_h = h;
    }
}

static void label_mission(rc_cpu *c)
{
    uint32_t ebp = c->r[RC_EBP];
    uint32_t p = rc_ld32(ebp - 0x1c); /* the entry's height; the width before it */
    uint32_t w = rc_ld32(p - 4), h = rc_ld32(p);
    if (w == 0 && h >= 1 && h <= NFACTORS) {
        window_label((char *)rc_ptr(ebp - 0x94), 0x40, h);
    }
}

static void label_menu(rc_cpu *c)
{
    const build *b = FIND(c, label_menu);
    if (b) {
        init_settings(b);
        sync_settings(b); /* a choice of the list, before the page is drawn again */
    }
    uint32_t e = c->r[RC_EDI] + 0x10;
    uint32_t w = rc_ld32(e), h = rc_ld32(e + 4);
    if (w == 0 && h >= 1 && h <= NFACTORS) {
        window_label((char *)rc_ptr(c->r[RC_EBP] - 0xa8), 0x40, h);
    }
}

static void label_screen(rc_cpu *c)
{
    /* sprintf(buffer, the text's "%d * %d pixel - %d bit color depth", width, height, depth) */
    uint32_t sp = c->r[RC_ESP];
    uint32_t w = rc_ld32(sp + 8), h = rc_ld32(sp + 12);
    int k = window_entry();
    if (w == 0 && h >= 1 && h <= NFACTORS) {
        window_label((char *)rc_ptr(rc_ld32(sp)), 0x40, h);
        rc_st8(c->r[RC_EBP] - 0x10, (uint8_t)(k == (int)h)); /* the page marks the mode in use */
    } else if (k) {
        rc_st8(c->r[RC_EBP] - 0x10, 0); /* a size the window has now, not the choice */
    }
}

static void start(rc_cpu *c)
{
    const build *b = FIND(c, start);
    if (b) {
        init_settings(b);
        sync_settings(b);
    }
}

/* The main loop: in a mission at a window size, follow the window. */
static void loop(rc_cpu *c)
{
    static int pending_w, pending_h;
    static uint64_t since;
    const build *b = FIND(c, loop);
    if (!b) {
        return;
    }
    sync_settings(b); /* a choice of the main menu's list, once the game has it */
    if (rc_ld32(b->mission) != 1) {
        pending_w = 0;
        return;
    }
    int k = window_entry(), w, h;
    if (!k || !entry_size(k, &w, &h)) {
        return;
    }
    uint32_t gfx = rc_ld32(b->app + 0x30);
    uint32_t disp = gfx ? rc_ld32(gfx + 0xc) : 0;
    if (!disp || (rc_ld32(disp + 4) == (uint32_t)w && rc_ld32(disp + 8) == (uint32_t)h)) {
        pending_w = 0;
        return;
    }
    uint64_t now = rc_ticks_peek_ms();
    if (w != pending_w || h != pending_h) {
        pending_w = w;
        pending_h = h;
        since = now;
        return;
    }
    if (now - since < SETTLE_MS) {
        return;
    }
    pending_w = 0;
    /* change(app; 0, k, 16, 0): the start of it makes the entry the window's size. */
    uint32_t r[8];
    memcpy(r, c->r, sizeof r);
    uint8_t flags[7] = {c->cf, c->pf, c->af, c->zf, c->sf, c->of, c->df};
    c->r[RC_ECX] = b->app;
    uint32_t args[4] = {0, (uint32_t)k, 16, 0};
    rc_call_guest(c, b->change, 4, args);
    memcpy(c->r, r, sizeof r);
    c->cf = flags[0], c->pf = flags[1], c->af = flags[2], c->zf = flags[3], c->sf = flags[4], c->of = flags[5],
    c->df = flags[6];
}

void cultures1_patch_resolution(void)
{
    /* The lists offer the window's size as their own entries. */
    rc_ddraw_list_settings_size(false);
    for (size_t i = 0; i < NBUILDS; i++) {
        rc_on_point(builds[i].settings_read, settings_read);
        rc_on_point(builds[i].checked, checked);
        rc_on_point(builds[i].change, change);
        rc_on_point(builds[i].enumerated, enumerated);
        rc_on_point(builds[i].label_mission, label_mission);
        rc_on_point(builds[i].label_menu, label_menu);
        rc_on_point(builds[i].label_screen, label_screen);
        rc_on_point(builds[i].loop, loop);
        rc_on_point(builds[i].click, click);
        rc_on_point(builds[i].start, start);
    }
}
