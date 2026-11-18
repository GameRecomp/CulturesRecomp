/* SPDX-License-Identifier: MIT */
/* kernel32.dll: so far only the clock. File system, heap, threads etc. follow in M5. */
#include "rc/hle.h"
#include "rc/hle_modules.h"
#include "rc/platform.h"

/* DWORD WINAPI GetTickCount(void) */
static void hle_kernel32_GetTickCount(rc_cpu *c) {
    rc_hle_return(c, (uint32_t)rc_platform_ticks_ms(), 0);
}

void rc_hle_register_kernel32(void) {
    rc_hle_register("kernel32.dll", "GetTickCount", 0, hle_kernel32_GetTickCount);
}
