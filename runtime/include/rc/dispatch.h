/* SPDX-License-Identifier: MIT */
/* Guest address -> host function. Indirect calls and jumps of lifted code, vtable calls and
 * all imports go through rc_dispatch(). Two sources: the registry (HLE thunks and anything
 * registered at run time) and the tables of lifted functions that `recomp lift` generates,
 * one per lifted binary. An unknown target is fatal and names the address, which then gets
 * added to the manifest as a discovery seed. */
#ifndef RC_DISPATCH_H
#define RC_DISPATCH_H

#include "rc/cpu.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register fn for guest address addr (non-zero). `name` may be NULL; it must stay valid.
 * Re-registering an address replaces the entry. Returns 0 on success. */
int rc_dispatch_register(uint32_t addr, rc_fn fn, const char *name);
rc_fn rc_dispatch_lookup(uint32_t addr);
const char *rc_dispatch_name(uint32_t addr);
size_t rc_dispatch_count(void);
void rc_dispatch_reset(void);

/* One lifted function. A generated table is sorted by address. */
typedef struct rc_fn_entry {
    uint32_t addr;
    rc_fn fn;
} rc_fn_entry;

/* Add the generated table of one lifted binary (at most 16). Returns 0 on success. */
int rc_dispatch_add_table(const rc_fn_entry *table, uint32_t count);
/* Forget all lifted tables. */
void rc_dispatch_clear_tables(void);

/* The function for `addr`: the registry first, then the lifted tables. NULL if none. */
rc_fn rc_lookup(uint32_t addr);

/* Call the function for target, with c->eip = target. Fatal if there is none. */
void rc_dispatch(rc_cpu *c, uint32_t target);

/* Patch points: `fn` runs before the instruction at `addr` when lifted code reaches an
 * rc_point() for it. Unused until a manifest lists patch points. */
void rc_on_point(uint32_t addr, rc_fn fn);

#ifdef __cplusplus
}
#endif
#endif
