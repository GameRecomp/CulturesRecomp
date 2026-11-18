/* SPDX-License-Identifier: MIT */
#if !defined(_WIN32)
#define _DEFAULT_SOURCE 1 /* MAP_ANONYMOUS on glibc */
#define _DARWIN_C_SOURCE 1
#endif

#include "rc/mem.h"
#include "rc/log.h"

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

uint8_t *rc_mem_base;

#define GUEST_SPACE (UINT64_C(1) << 32)
/* Unmapped slack after 4 GiB, so an access straddling 0xffffffff faults instead of wrapping. */
#define GUARD_SIZE UINT64_C(0x10000)
#define GUEST_PAGES (1u << 20)

static uint8_t page_bits[GUEST_PAGES / 8];
static uint64_t host_page = RC_PAGE_SIZE;

static void mark(uint64_t start, uint64_t end, int mapped) {
    for (uint64_t p = start / RC_PAGE_SIZE; p < end / RC_PAGE_SIZE; p++) {
        if (mapped)
            page_bits[p >> 3] |= (uint8_t)(1u << (p & 7));
        else
            page_bits[p >> 3] &= (uint8_t)~(1u << (p & 7));
    }
}

/* Widen [addr, addr+size) to host pages. Returns -1 if it leaves the guest space. */
static int host_range(uint32_t addr, uint32_t size, uint64_t *start, uint64_t *end) {
    uint64_t s = (uint64_t)addr & ~(host_page - 1);
    uint64_t e = ((uint64_t)addr + size + host_page - 1) & ~(host_page - 1);
    if (e > GUEST_SPACE) return -1;
    *start = s;
    *end = e;
    return 0;
}

int rc_mem_init(void) {
#if UINTPTR_MAX <= 0xffffffffu
    RC_ERROR("guest memory needs a 64-bit host");
    return -1;
#else
    if (rc_mem_base) return 0;
    const uint16_t probe = 1;
    if (*(const uint8_t *)&probe != 1) {
        RC_ERROR("big-endian hosts are not supported");
        return -1;
    }
    const size_t total = (size_t)(GUEST_SPACE + GUARD_SIZE);
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    host_page = si.dwPageSize;
    void *p = VirtualAlloc(NULL, total, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) {
        RC_ERROR("cannot reserve 4 GiB of guest address space");
        return -1;
    }
#else
    long ps = sysconf(_SC_PAGESIZE);
    host_page = ps > 0 ? (uint64_t)ps : RC_PAGE_SIZE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_NORESERVE
    flags |= MAP_NORESERVE;
#endif
    void *p = mmap(NULL, total, PROT_NONE, flags, -1, 0);
    if (p == MAP_FAILED) {
        RC_ERROR("cannot reserve 4 GiB of guest address space");
        return -1;
    }
#endif
    if (host_page < RC_PAGE_SIZE || (host_page & (host_page - 1)) != 0) {
        RC_ERROR("unsupported host page size %llu", (unsigned long long)host_page);
        return -1;
    }
    rc_mem_base = (uint8_t *)p;
    for (size_t i = 0; i < sizeof page_bits; i++) page_bits[i] = 0;
    return 0;
#endif
}

void rc_mem_shutdown(void) {
    if (!rc_mem_base) return;
#if defined(_WIN32)
    VirtualFree(rc_mem_base, 0, MEM_RELEASE);
#else
    munmap(rc_mem_base, (size_t)(GUEST_SPACE + GUARD_SIZE));
#endif
    rc_mem_base = NULL;
}

int rc_mem_map(uint32_t addr, uint32_t size) {
    uint64_t s, e;
    if (!rc_mem_base || size == 0 || host_range(addr, size, &s, &e) != 0) return -1;
#if defined(_WIN32)
    if (!VirtualAlloc(rc_mem_base + s, (size_t)(e - s), MEM_COMMIT, PAGE_READWRITE)) return -1;
#else
    if (mprotect(rc_mem_base + s, (size_t)(e - s), PROT_READ | PROT_WRITE) != 0) return -1;
#endif
    mark(s, e, 1);
    return 0;
}

int rc_mem_unmap(uint32_t addr, uint32_t size) {
    uint64_t s, e;
    if (!rc_mem_base || size == 0 || host_range(addr, size, &s, &e) != 0) return -1;
#if defined(_WIN32)
    if (!VirtualFree(rc_mem_base + s, (size_t)(e - s), MEM_DECOMMIT)) return -1;
#else
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
#ifdef MAP_NORESERVE
    flags |= MAP_NORESERVE;
#endif
    /* Replacing the pages drops their contents; a later map sees zeroes again. */
    if (mmap(rc_mem_base + s, (size_t)(e - s), PROT_NONE, flags, -1, 0) == MAP_FAILED) return -1;
#endif
    mark(s, e, 0);
    return 0;
}

int rc_mem_is_mapped(uint32_t addr) {
    uint32_t p = addr / RC_PAGE_SIZE;
    return rc_mem_base && (page_bits[p >> 3] >> (p & 7) & 1u);
}
