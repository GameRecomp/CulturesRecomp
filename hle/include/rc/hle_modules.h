/* SPDX-License-Identifier: MIT */
/* The HLE modules: one per Windows DLL the games import or load at run time. */
#ifndef RC_HLE_MODULES_H
#define RC_HLE_MODULES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rc_hle_kind {
    RC_HLE_SYSTEM,     /* reimplemented Windows/DirectX DLL */
    RC_HLE_MIDDLEWARE, /* reimplemented third-party DLL (Miles) */
    RC_HLE_LIFTED      /* shipped with the game and statically recompiled, not HLE */
} rc_hle_kind;

typedef struct rc_hle_module {
    const char *dll;          /* lower case, as normalised by rc_hle_register() */
    rc_hle_kind kind;
    void (*register_fn)(void); /* NULL until the module has an implementation */
} rc_hle_module;

extern const rc_hle_module rc_hle_modules[];
extern const size_t rc_hle_module_count;

/* Find a module by DLL name (case-insensitive, ".dll" optional). NULL if unknown. */
const rc_hle_module *rc_hle_find_module(const char *dll);

/* Register every implemented module with the runtime's HLE registry. */
void rc_hle_register_all(void);

/* Per-module registration functions. */
void rc_hle_register_kernel32(void);
void rc_hle_register_winmm(void);

#ifdef __cplusplus
}
#endif
#endif
