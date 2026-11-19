/* SPDX-License-Identifier: MIT */
/*
 * DirectSound: for now no device (DSERR_NODRIVER), so a game that also has another way to
 * play sound (Cultures 1: Miles) uses that. The buffers on the platform's mixer are milestone
 * M7 (and IDirectSound8 for the C2 engine, M11).
 */
#include "dsound.h"

#define DS "DSOUND.dll"
#define DSERR_NODRIVER 0x88780078u

/* DirectSoundCreate(guid, result, outer): ordinal 1. */
static void k_DirectSoundCreate(rc_cpu *c)
{
    if (RC_ARG(c, 1)) {
        rc_st32(RC_ARG(c, 1), 0);
    }
    RC_RETURN(c, DSERR_NODRIVER, 3);
}

/* DirectSoundEnumerateA(callback, context): no devices. */
static void k_DirectSoundEnumerateA(rc_cpu *c) { RC_RETURN(c, 0, 2); }

const rc_import_def rc_dsound_defs[] = {
    {DS, "#1", k_DirectSoundCreate},
    {DS, "#11", k_DirectSoundCreate},
    {DS, "DirectSoundCreate8", k_DirectSoundCreate},
    {DS, "DirectSoundCreate", k_DirectSoundCreate},
    {DS, "#2", k_DirectSoundEnumerateA},
    {DS, "DirectSoundEnumerateA", k_DirectSoundEnumerateA},
};
const uint32_t rc_dsound_count = sizeof rc_dsound_defs / sizeof rc_dsound_defs[0];
