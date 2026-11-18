/* SPDX-License-Identifier: MIT */
#include "rc/dispatch.h"
#include "rc/log.h"

#include <stdlib.h>

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

void rc_dispatch(rc_cpu *c, uint32_t target) {
    entry *e = find(target);
    if (!e) {
        rc_fatal("dispatch: no function at guest address 0x%08x (esp 0x%08x, from eip 0x%08x)",
                 target, c->esp, c->eip);
    }
    c->eip = target;
    e->fn(c);
}
