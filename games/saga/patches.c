/* SPDX-License-Identifier: MIT */
/*
 * The patches of this game on the Cultures 2 engine: the engine's, shared with the other games on it
 * (games/c2engine/), with the addresses for the lifter under [binary.patch] in game.toml.
 */
#include "../c2engine/c2engine.h"

void saga_patches(void);

void saga_patches(void)
{
    c2engine_patch_resolution();
}
