/* SPDX-License-Identifier: MIT */
/*
 * Cultures 1's patches: where this project changes what the game does, each in a file of its own
 * (patches.h), with the addresses for the lifter under [binary.patch] in game.toml, as in
 * TropicoRecomp. How each was found is in docs/RESEARCH.md ("Resolutions").
 */
#include "patches.h"

void cultures1_patches(void)
{
    cultures1_patch_resolution();
    cultures1_patch_cursor();
}
