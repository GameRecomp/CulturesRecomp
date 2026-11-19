/* SPDX-License-Identifier: MIT */
#include "rc/dispatch.h"
#include "rc/lifted.h"
#include "rc/log.h"

#include <stdlib.h>
#include <time.h>

typedef struct entry {
    uint32_t addr; /* 0 = empty slot */
    rc_fn fn;
    const char *name;
} entry;

static entry *table;
static size_t capacity; /* power of two */
static size_t used;

static size_t slot_of(uint32_t addr) { return (size_t)(addr * 2654435761u) & (capacity - 1); }

static entry *find(uint32_t addr) {
    if (!capacity || !addr) return NULL;
    for (size_t i = slot_of(addr);; i = (i + 1) & (capacity - 1)) {
        if (table[i].addr == addr) return &table[i];
        if (table[i].addr == 0) return NULL;
    }
}

static int grow(void) {
    size_t new_cap = capacity ? capacity * 2 : 4096;
    entry *old = table;
    size_t old_cap = capacity;
    table = calloc(new_cap, sizeof *table);
    if (!table) {
        table = old;
        return -1;
    }
    capacity = new_cap;
    for (size_t i = 0; i < old_cap; i++) {
        if (!old[i].addr) continue;
        size_t j = slot_of(old[i].addr);
        while (table[j].addr) j = (j + 1) & (capacity - 1);
        table[j] = old[i];
    }
    free(old);
    return 0;
}

int rc_dispatch_register(uint32_t addr, rc_fn fn, const char *name) {
    if (!addr || !fn) return -1;
    entry *e = find(addr);
    if (e) {
        e->fn = fn;
        e->name = name;
        return 0;
    }
    if ((used + 1) * 2 > capacity && grow() != 0) return -1;
    size_t i = slot_of(addr);
    while (table[i].addr) i = (i + 1) & (capacity - 1);
    table[i].addr = addr;
    table[i].fn = fn;
    table[i].name = name;
    used++;
    return 0;
}

rc_fn rc_dispatch_lookup(uint32_t addr) {
    entry *e = find(addr);
    return e ? e->fn : NULL;
}

const char *rc_dispatch_name(uint32_t addr) {
    entry *e = find(addr);
    return e ? e->name : NULL;
}

size_t rc_dispatch_count(void) { return used; }

void rc_dispatch_reset(void) {
    free(table);
    table = NULL;
    capacity = 0;
    used = 0;
}

/* Lifted code: one sorted table per binary. */
#define MAX_TABLES 16
static struct {
    const rc_fn_entry *entries;
    uint32_t count;
} tables[MAX_TABLES];
static uint32_t ntables;

int rc_dispatch_add_table(const rc_fn_entry *t, uint32_t count) {
    if (ntables == MAX_TABLES) return -1;
    tables[ntables].entries = t;
    tables[ntables].count = count;
    ntables++;
    return 0;
}

void rc_dispatch_clear_tables(void) { ntables = 0; }

static rc_fn lookup_lifted(uint32_t addr) {
    for (uint32_t k = 0; k < ntables; k++) {
        const rc_fn_entry *t = tables[k].entries;
        uint32_t lo = 0, hi = tables[k].count;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (t[mid].addr < addr) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo < tables[k].count && t[lo].addr == addr) return t[lo].fn;
    }
    return NULL;
}

rc_fn rc_lookup(uint32_t addr) {
    entry *e = find(addr);
    return e ? e->fn : lookup_lifted(addr);
}

void rc_dispatch(rc_cpu *c, uint32_t target) {
    rc_fn f = rc_lookup(target);
    if (!f) {
        uint32_t ret = rc_mem_is_mapped(c->esp) && rc_mem_is_mapped(c->esp + 3) ? rc_ld32(c->esp) : 0;
        rc_fatal("dispatch: no function at guest address 0x%08x (esp 0x%08x, return address "
                 "0x%08x, from eip 0x%08x)",
                 target, c->esp, ret, c->eip);
    }
    c->eip = target;
    f(c);
}

void rc_trap(rc_cpu *c, uint32_t addr, const char *why) {
    c->eip = addr;
    rc_fatal("%s (at 0x%08x; eax %#x ecx %#x edx %#x ebx %#x esp %#x ebp %#x esi %#x edi %#x)",
             why, addr, c->eax, c->ecx, c->edx, c->ebx, c->esp, c->ebp, c->esi, c->edi);
}

#define MAX_POINTS 64
static struct {
    uint32_t addr;
    rc_fn fn;
} points[MAX_POINTS];
static uint32_t npoints;

void rc_on_point(uint32_t addr, rc_fn fn) {
    for (uint32_t i = 0; i < npoints; i++) {
        if (points[i].addr == addr) {
            points[i].fn = fn;
            return;
        }
    }
    if (npoints == MAX_POINTS) rc_fatal("too many patch points");
    points[npoints].addr = addr;
    points[npoints].fn = fn;
    npoints++;
}

void rc_point(rc_cpu *c, uint32_t addr) {
    for (uint32_t i = 0; i < npoints; i++) {
        if (points[i].addr == addr) {
            points[i].fn(c);
            return;
        }
    }
}

/* A fixed CPU, as in TropicoRecomp: a P6-class Intel processor with FPU, TSC, CX8 and CMOV,
 * and no MMX or SSE, so libraries of the time (Bink) take their plain code paths. */
void rc_cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    switch (*eax) {
    case 0:
        *eax = 1;
        *ebx = 0x756e6547; /* "Genu" */
        *edx = 0x49656e69; /* "ineI" */
        *ecx = 0x6c65746e; /* "ntel" */
        break;
    case 1:
        *eax = 0x00000683; /* family 6, model 8, stepping 3 */
        *ebx = 0;
        *ecx = 0;
        *edx = 0x00008111;
        break;
    default:
        *eax = *ebx = *ecx = *edx = 0;
        break;
    }
}

/* Nanoseconds since an arbitrary start: a 1 GHz time-stamp counter. */
void rc_rdtsc(uint32_t *eax, uint32_t *edx) {
    struct timespec ts;
    uint64_t t = 0;
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        t = (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
    }
    *eax = (uint32_t)t;
    *edx = (uint32_t)(t >> 32);
}
