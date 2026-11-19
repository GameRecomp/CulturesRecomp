/* SPDX-License-Identifier: MIT */
/* Guest CPU state shared by lifted code, the dispatcher and the HLE layer.
 *
 * Every lifted function is `void <prefix><address>(rc_cpu *c)`. The state is passed
 * explicitly, so lifted code is re-entrant: the host calls back into the guest (window
 * procedures, sound callbacks) while guest code is on the C stack. Lifted functions keep
 * registers and flags in C locals and write them back to rc_cpu at calls, returns and
 * indirect transfers. The guest stack lives in guest memory, so `esp` is a real guest
 * address and every calling convention works unchanged.
 *
 * The layout is TropicoRecomp's (the lifter and the C emitter are shared with it): the
 * registers in encoding order, one byte per arithmetic flag, the x87 in the fast mode. */
#ifndef RC_CPU_H
#define RC_CPU_H

#include "rc/mem.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RC_EAX, RC_ECX, RC_EDX, RC_EBX, RC_ESP, RC_EBP, RC_ESI, RC_EDI };

typedef struct rc_cpu {
    /* General-purpose registers: `r[RC_EAX]` in generated code, `eax` in hand-written code. */
    union {
        uint32_t r[8];
        struct {
            uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
        };
    };

    /* Arithmetic flags, one byte each (0 or 1), so generated code sets and tests them
     * without masking. */
    uint8_t cf, pf, af, zf, sf, of;
    /* Direction flag for string instructions. */
    uint8_t df;
    uint8_t _pad;

    /* x87 in the fast mode (rc/x87.h): eight registers as host doubles, the top of stack,
     * and the control, status and tag words. */
    double st[8];
    uint8_t top;
    uint16_t fpu_control;
    uint16_t fpu_status;
    uint16_t fpu_tag;

    /* Guest address of the TEB, which `fs:` addresses. */
    uint32_t fs_base;

    /* Target of the current dispatch. Set by rc_dispatch() so callees (HLE thunks, traps)
     * know which guest address they were reached through. */
    uint32_t eip;
} rc_cpu;

/* Guest function / HLE entry point. */
typedef void (*rc_fn)(rc_cpu *c);

/* Reset all registers. The FPU starts in the Win32 default state (CW 0x027f, all empty). */
void rc_cpu_init(rc_cpu *c, uint32_t esp);

/* EFLAGS as the guest sees it (pushfd, lahf). */
static inline uint32_t rc_eflags(const rc_cpu *c) {
    return (uint32_t)c->cf | 0x2u | ((uint32_t)c->pf << 2) | ((uint32_t)c->af << 4) |
           ((uint32_t)c->zf << 6) | ((uint32_t)c->sf << 7) | ((uint32_t)c->df << 10) |
           ((uint32_t)c->of << 11);
}

static inline void rc_set_eflags(rc_cpu *c, uint32_t f) {
    c->cf = f & 1;
    c->pf = (f >> 2) & 1;
    c->af = (f >> 4) & 1;
    c->zf = (f >> 6) & 1;
    c->sf = (f >> 7) & 1;
    c->df = (f >> 10) & 1;
    c->of = (f >> 11) & 1;
}

static inline void rc_push32(rc_cpu *c, uint32_t v) {
    c->esp -= 4;
    rc_st32(c->esp, v);
}
static inline uint32_t rc_pop32(rc_cpu *c) {
    uint32_t v = rc_ld32(c->esp);
    c->esp += 4;
    return v;
}

#ifdef __cplusplus
}
#endif
#endif
