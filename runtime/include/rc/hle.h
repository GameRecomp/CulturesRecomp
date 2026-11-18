/* SPDX-License-Identifier: MIT */
/* Registry of high-level emulated (HLE) imports.
 *
 * Every imported function gets a reserved guest address ("thunk") in a range the games never
 * use. The PE loader writes thunk addresses into the IAT, so `call [iat]` reaches
 * rc_dispatch() and then the HLE function.
 *
 * Calling convention inside an HLE function: on entry [esp] is the return address and the
 * arguments follow (rc_arg(c, 0) is the first one). A stdcall function pops the return
 * address and its arguments with rc_hle_return(); a cdecl one with rc_hle_return_cdecl(). */
#ifndef RC_HLE_H
#define RC_HLE_H

#include "rc/cpu.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC_HLE_THUNK_BASE 0xF0000000u
#define RC_HLE_THUNK_STRIDE 0x10u
#define RC_HLE_MAX_THUNKS 0x10000u

/* Register an HLE implementation. dll is matched case-insensitively ("KERNEL32.dll" ==
 * "kernel32.dll"). name may be NULL for ordinal-only exports; ordinal may be 0 if unknown.
 * Returns the thunk address, or 0 if the table is full. */
uint32_t rc_hle_register(const char *dll, const char *name, uint16_t ordinal, rc_fn fn);

/* Thunk address for an import by name or ordinal. Unknown imports get a trap thunk that is
 * fatal when called (naming DLL and function), so a game can load with gaps and fail only
 * when it actually uses one. Never returns 0. */
uint32_t rc_hle_resolve(const char *dll, const char *name);
uint32_t rc_hle_resolve_ordinal(const char *dll, uint16_t ordinal);

/* Non-zero if the import has a real implementation (not a trap). */
int rc_hle_is_implemented(const char *dll, const char *name);
size_t rc_hle_count(void);
void rc_hle_reset(void);

/* i-th 32-bit stack argument at function entry. */
static inline uint32_t rc_arg(const rc_cpu *c, unsigned i) { return rc_ld32(c->esp + 4u + 4u * i); }

/* stdcall return: set eax, pop the return address and nargs arguments. */
static inline void rc_hle_return(rc_cpu *c, uint32_t eax, unsigned nargs) {
    c->eax = eax;
    c->esp += 4u + 4u * nargs;
}
/* cdecl return: set eax, pop only the return address. */
static inline void rc_hle_return_cdecl(rc_cpu *c, uint32_t eax) {
    c->eax = eax;
    c->esp += 4u;
}

#ifdef __cplusplus
}
#endif
#endif
