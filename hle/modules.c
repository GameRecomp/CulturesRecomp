/* SPDX-License-Identifier: MIT */
#include "rc/hle_modules.h"

#include <string.h>

/* Every DLL imported by a pinned binary (docs/TARGETS.md) or loaded with LoadLibrary
 * (ddraw, d3d8, dplayx). The test in hle/tests checks this list against the targets. */
const rc_hle_module rc_hle_modules[] = {
    {"kernel32.dll", RC_HLE_SYSTEM, rc_hle_register_kernel32},
    {"user32.dll", RC_HLE_SYSTEM, NULL},
    {"gdi32.dll", RC_HLE_SYSTEM, NULL},
    {"advapi32.dll", RC_HLE_SYSTEM, NULL},
    {"shell32.dll", RC_HLE_SYSTEM, NULL},
    {"shlwapi.dll", RC_HLE_SYSTEM, NULL},
    {"ole32.dll", RC_HLE_SYSTEM, NULL},
    {"version.dll", RC_HLE_SYSTEM, NULL},
    {"winmm.dll", RC_HLE_SYSTEM, rc_hle_register_winmm},
    {"ddraw.dll", RC_HLE_SYSTEM, NULL},
    {"d3d8.dll", RC_HLE_SYSTEM, NULL},
    {"dinput.dll", RC_HLE_SYSTEM, NULL},
    {"dsound.dll", RC_HLE_SYSTEM, NULL},
    {"dplayx.dll", RC_HLE_SYSTEM, NULL},
    {"wsock32.dll", RC_HLE_SYSTEM, NULL},
    {"ws2_32.dll", RC_HLE_SYSTEM, NULL},
    {"comctl32.dll", RC_HLE_SYSTEM, NULL},
    {"comdlg32.dll", RC_HLE_SYSTEM, NULL},
    {"winspool.drv", RC_HLE_SYSTEM, NULL},
    {"mss32.dll", RC_HLE_MIDDLEWARE, NULL},
    {"binkw32.dll", RC_HLE_LIFTED, NULL},
    {"dplaydll.dll", RC_HLE_LIFTED, NULL},
    {"gedx8musicdrv.dll", RC_HLE_LIFTED, NULL},
};
const size_t rc_hle_module_count = sizeof rc_hle_modules / sizeof rc_hle_modules[0];

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

const rc_hle_module *rc_hle_find_module(const char *dll) {
    char n[64];
    size_t i = 0;
    int dot = 0;
    for (; dll[i] && i + 5 < sizeof n; i++) {
        n[i] = lower(dll[i]);
        if (dll[i] == '.') dot = 1;
    }
    n[i] = '\0';
    if (!dot) strcat(n, ".dll");
    for (size_t k = 0; k < rc_hle_module_count; k++) {
        if (strcmp(rc_hle_modules[k].dll, n) == 0) return &rc_hle_modules[k];
    }
    return NULL;
}

void rc_hle_register_all(void) {
    for (size_t k = 0; k < rc_hle_module_count; k++) {
        if (rc_hle_modules[k].register_fn) rc_hle_modules[k].register_fn();
    }
}
