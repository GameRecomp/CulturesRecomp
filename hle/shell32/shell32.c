/* SPDX-License-Identifier: MIT */
/*
 * SHELL32: ShellExecuteA, which games use to open their web site or manual. Nothing is
 * started from here; the call reports success (a value above 32) so the game goes on.
 */
#include "shell32.h"

#include "rc/log.h"

#define SH "SHELL32.dll"

/* ShellExecuteA(hwnd, operation, file, parameters, directory, show) */
static void k_ShellExecuteA(rc_cpu *c)
{
    const char *op = rc_str(RC_ARG(c, 1)), *file = rc_str(RC_ARG(c, 2));
    rc_log("ShellExecuteA(%s, \"%s\"): not started", op ? op : "open", file ? file : "");
    RC_RETURN(c, 42, 6);
}

const rc_import_def rc_shell32_defs[] = {
    {SH, "ShellExecuteA", k_ShellExecuteA},
};
const uint32_t rc_shell32_count = sizeof rc_shell32_defs / sizeof rc_shell32_defs[0];
