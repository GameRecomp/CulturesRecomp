/* SPDX-License-Identifier: MIT */
/*
 * The resolution of the settings (rc_plat_game_size: the window's or the screen's size, divided by
 * game_scale, or a size of the player's choosing) for the games on the Cultures 2 engine.
 *
 * The engine takes its screen size from game.ini (`gfx_screen_width`, `gfx_screen_height`; 800 x
 * 600 when they are missing) in its start-up function and lays out its menus and the game screen
 * for any size, widescreen too. The function keeps the two values in locals and uses them from
 * the instruction after the depth's: the point there (`[binary.patch] points`) puts the chosen size
 * into those locals. DirectDraw lists the size among its modes (hle/ddraw), so the engine's check
 * of the mode passes. docs/RESEARCH.md, "Resolutions".
 */
#include <stddef.h>

#include "c2engine.h"
#include "rc/cpu.h"
#include "rc/dispatch.h"
#include "rc/log.h"
#include "rc/mem.h"
#include "rc/platform.h"

/* The point in each build, and where the width and the height are (ebp-relative). */
static const struct {
    uint32_t point;
    int8_t width, height;
} builds[] = {
    {0x00401259, -0x38, -0x34}, /* Cultures2.exe (GOG EN, DE) */
    {0x00401261, -0x3c, -0x38}, /* Northland Game.exe (GOG EN, ES) */
    {0x00401255, -0x38, -0x34}, /* 8th Wonder Game.exe (GOG EN, DE); the Saga's Game.exe */
    {0x0040125d, -0x3c, -0x38}, /* 8th Wonder Game.exe (GOG ES); the Saga's GameSaga.exe */
};

static void screen_size(rc_cpu *c)
{
    int w, h;
    if (!rc_plat_game_size(&w, &h)) {
        return; /* resolution = off: game.ini's */
    }
    for (size_t i = 0; i < sizeof builds / sizeof builds[0]; i++) {
        if (builds[i].point == c->eip) {
            uint32_t ebp = c->r[RC_EBP];
            rc_st32(ebp + (uint32_t)(int32_t)builds[i].width, (uint32_t)w);
            rc_st32(ebp + (uint32_t)(int32_t)builds[i].height, (uint32_t)h);
            rc_log("resolution: %dx%d", w, h);
            return;
        }
    }
}

void c2engine_patch_resolution(void)
{
    for (size_t i = 0; i < sizeof builds / sizeof builds[0]; i++) {
        rc_on_point(builds[i].point, screen_size);
    }
}
