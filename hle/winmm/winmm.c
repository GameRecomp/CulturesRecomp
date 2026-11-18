/* SPDX-License-Identifier: MIT */
/* winmm.dll: so far only the clock. Mixer stubs and MCI CD audio follow in M7. */
#include "rc/hle.h"
#include "rc/hle_modules.h"
#include "rc/platform.h"

/* DWORD WINAPI timeGetTime(void) */
static void hle_winmm_timeGetTime(rc_cpu *c) {
    rc_hle_return(c, (uint32_t)rc_platform_ticks_ms(), 0);
}

void rc_hle_register_winmm(void) {
    rc_hle_register("winmm.dll", "timeGetTime", 0, hle_winmm_timeGetTime);
}
