/* SPDX-License-Identifier: MIT */
/*
 * Cultures 1 plays a mission at the resolution of the settings (rc_plat_game_size); its menus keep
 * their 800x600 (640x480 for the intro).
 *
 * The game reads its settings from the registry into an object (esi at the point): the width at
 * +0x11, the height at +0x13 (16-bit), the colour depth at +0x15. Then it checks the mode against
 * the modes DirectDraw listed and the desktop's size, and falls back to a stock size when that
 * fails. The point before that check puts the chosen size into the object; DirectDraw lists it
 * and GDI's desktop is as large (hle/ddraw, hle/gdi32). The game draws its world at any size. The
 * size is not written back to the registry: the options page keeps working with the game's own.
 */
#include "patches.h"
#include "rc/cpu.h"
#include "rc/dispatch.h"
#include "rc/log.h"
#include "rc/mem.h"
#include "rc/platform.h"

static void settings_read(rc_cpu *c)
{
    int w, h;
    if (!rc_plat_game_size(&w, &h)) {
        return; /* resolution = off: the registry's */
    }
    uint32_t s = c->r[RC_ESI];
    rc_st16(s + 0x11, (uint16_t)w);
    rc_st16(s + 0x13, (uint16_t)h);
    rc_log("resolution: %dx%d", w, h);
}

void cultures1_patch_resolution(void)
{
    rc_on_point(0x00466c24, settings_read); /* GOG EN, DE */
    rc_on_point(0x00467712, settings_read); /* Gold DE */
}
