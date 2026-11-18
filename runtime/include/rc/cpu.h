/* SPDX-License-Identifier: MIT */
/* Guest CPU state shared by lifted code, the dispatcher and the HLE layer.
 *
 * Lifted functions keep registers and flags in C locals and write them back to rc_cpu
 * at calls, returns and indirect jumps. The guest stack lives in guest memory, so `esp`
 * is a real guest address and every calling convention works unchanged. */
#ifndef RC_CPU_H
#define RC_CPU_H

#include "rc/mem.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EFLAGS bits the lifter models. */
enum {
    RC_FLAG_CF = 1u << 0,
    RC_FLAG_PF = 1u << 2,
    RC_FLAG_AF = 1u << 4,
    RC_FLAG_ZF = 1u << 6,
    RC_FLAG_SF = 1u << 7,
    RC_FLAG_DF = 1u << 10,
    RC_FLAG_OF = 1u << 11
};

typedef struct rc_x87 {
    double st[8]; /* physical registers; ST(i) = st[(top + i) & 7] */
    uint16_t cw;  /* control word */
    uint16_t sw;  /* status word (without TOP) */
    uint16_t tag; /* 2 bits per register, 3 = empty */
    uint8_t top;
} rc_x87;

typedef struct rc_cpu {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t eflags;
    /* Target of the current dispatch. Set by rc_dispatch() so callees (HLE thunks, traps)
     * know which guest address they were reached through. */
    uint32_t eip;
    uint32_t fs_base; /* guest address of the TEB */
    rc_x87 fpu;
} rc_cpu;

/* Guest function / HLE entry point. */
typedef void (*rc_fn)(rc_cpu *c);

/* Reset all registers. The FPU starts in the Win32 default state (CW 0x027f, all empty). */
void rc_cpu_init(rc_cpu *c, uint32_t esp);

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
