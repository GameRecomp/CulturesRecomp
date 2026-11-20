/* SPDX-License-Identifier: MIT */
/*
 * The system's mouse pointer instead of the one the game draws (settings.ini hardware_cursor, on
 * by default). The game's "Hardware Mouse" registry value chooses; GOG's installer writes 0. The
 * point is just after the game asked the registry for it (eax: the result, [ebp-4]: the value): it
 * makes the answer "found, 1", so the game switches to the system's pointer itself.
 */
#include "patches.h"
#include "rc/cpu.h"
#include "rc/dispatch.h"
#include "rc/mem.h"
#include "rc/platform.h"

static void hardware_mouse_read(rc_cpu *c)
{
    if (!rc_plat_display_get().hardware_cursor) {
        return;
    }
    c->r[RC_EAX] = 0; /* ERROR_SUCCESS */
    rc_st32(c->r[RC_EBP] - 4, 1);
}

void cultures1_patch_cursor(void)
{
    rc_on_point(0x00466b44, hardware_mouse_read); /* GOG EN, DE */
    rc_on_point(0x00467632, hardware_mouse_read); /* Gold DE */
}
