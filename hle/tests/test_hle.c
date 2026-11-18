/* SPDX-License-Identifier: MIT */
#include "rc/cpu.h"
#include "rc/dispatch.h"
#include "rc/hle.h"
#include "rc/hle_modules.h"
#include "rc/mem.h"
#include "rc_test.h"

/* Every DLL imported by a binary with treatment "lift" in games/<id>/game.toml, plus the
 * ones loaded at run time. Keep in sync with docs/TARGETS.md. */
static const char *const imported[] = {
    "KERNEL32.dll", "USER32.dll", "GDI32.dll",    "ADVAPI32.dll", "SHELL32.dll",
    "SHLWAPI.dll",  "ole32.dll",  "VERSION.dll",  "WINMM.dll",    "DINPUT.dll",
    "DSOUND.dll",   "WSOCK32.dll", "WS2_32.dll",  "COMCTL32.dll", "comdlg32.dll",
    "WINSPOOL.DRV", "mss32.dll",  "binkw32.dll",  "DPlayDLL.dll", "DDRAW.DLL",
    "D3D8.DLL",     "dplayx.dll",
};

static void test_module_table(void) {
    for (size_t i = 0; i < sizeof imported / sizeof imported[0]; i++) {
        const rc_hle_module *m = rc_hle_find_module(imported[i]);
        if (!m) fprintf(stderr, "no HLE module for %s\n", imported[i]);
        CHECK(m != NULL);
    }
    CHECK(rc_hle_find_module("kernel32") != NULL);
    CHECK(rc_hle_find_module("nonexistent.dll") == NULL);
    CHECK(rc_hle_find_module("binkw32.dll")->kind == RC_HLE_LIFTED);
}

static void call_clock(const char *dll, const char *name) {
    rc_cpu c;
    rc_cpu_init(&c, 0x00110000);
    uint32_t thunk = rc_hle_resolve(dll, name);
    CHECK(rc_hle_is_implemented(dll, name));
    rc_push32(&c, 0x00401000); /* return address */
    c.eax = 0xffffffffu;
    rc_dispatch(&c, thunk);
    CHECK_EQ_U32(c.esp, 0x00110000);
    CHECK(c.eax != 0xffffffffu);
}

static void test_clock_functions(void) {
    rc_hle_reset();
    rc_hle_register_all();
    call_clock("KERNEL32.dll", "GetTickCount");
    call_clock("WINMM.dll", "timeGetTime");
    CHECK(!rc_hle_is_implemented("WINMM.dll", "mciSendCommandA"));
    rc_hle_reset();
}

int main(void) {
    if (rc_mem_init() != 0 || rc_mem_map(0x00100000, 0x10000) != 0) {
        fprintf(stderr, "cannot set up guest memory\n");
        return 1;
    }
    RUN(test_module_table);
    RUN(test_clock_functions);
    rc_mem_shutdown();
    return TEST_EXIT();
}
