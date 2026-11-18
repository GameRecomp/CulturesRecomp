/* SPDX-License-Identifier: MIT */
/* Guest memory: one flat 32-bit address space, reserved up front on 64-bit hosts.
 * Guest address `a` lives at host address `rc_mem_base + a`. Pages are committed on
 * demand by rc_mem_map(). Everything outside a mapping faults on the host, so a stray
 * guest pointer crashes at the right place instead of corrupting memory. */
#ifndef RC_MEM_H
#define RC_MEM_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC_PAGE_SIZE 0x1000u

extern uint8_t *rc_mem_base;

/* Reserve the 4 GiB guest space. Returns 0 on success. Idempotent. */
int rc_mem_init(void);
void rc_mem_shutdown(void);

/* Commit [addr, addr+size) read/write and zero-filled. The range is widened to host pages.
 * Returns 0 on success. Mapping an already mapped page keeps its contents. */
int rc_mem_map(uint32_t addr, uint32_t size);
/* Decommit a range (contents are lost). */
int rc_mem_unmap(uint32_t addr, uint32_t size);
/* Non-zero if the 4 KiB guest page containing addr is mapped. */
int rc_mem_is_mapped(uint32_t addr);

static inline uint8_t *rc_ptr(uint32_t a) { return rc_mem_base + a; }

/* Little-endian, unaligned-safe accessors (the supported hosts are little-endian). */
static inline uint8_t rc_ld8(uint32_t a) { return rc_mem_base[a]; }
static inline uint16_t rc_ld16(uint32_t a) {
    uint16_t v;
    memcpy(&v, rc_mem_base + a, sizeof v);
    return v;
}
static inline uint32_t rc_ld32(uint32_t a) {
    uint32_t v;
    memcpy(&v, rc_mem_base + a, sizeof v);
    return v;
}
static inline void rc_st8(uint32_t a, uint8_t v) { rc_mem_base[a] = v; }
static inline void rc_st16(uint32_t a, uint16_t v) { memcpy(rc_mem_base + a, &v, sizeof v); }
static inline void rc_st32(uint32_t a, uint32_t v) { memcpy(rc_mem_base + a, &v, sizeof v); }

#ifdef __cplusplus
}
#endif
#endif
