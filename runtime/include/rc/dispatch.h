/* SPDX-License-Identifier: MIT */
/* Guest address -> host function table. Indirect calls and jumps of lifted code, vtable
 * calls and all imports go through rc_dispatch(). An unknown target is fatal and names
 * the address, which then gets added to the manifest as a discovery seed. */
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

/* Call the function registered for target, with c->eip = target. Fatal if there is none. */
void rc_dispatch(rc_cpu *c, uint32_t target);

#ifdef __cplusplus
}
#endif
#endif
