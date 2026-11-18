/* SPDX-License-Identifier: MIT */
#include "rc/hle.h"
#include "rc/dispatch.h"
#include "rc/log.h"
#include "strutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct hle_entry {
    char *dll;   /* normalised: lower case, with extension */
    char *name;  /* NULL for ordinal-only */
    uint16_t ordinal;
    int implemented;
    uint32_t thunk;
    char *label; /* "dll!name" or "dll!#ordinal", referenced by the dispatch table */
} hle_entry;

static hle_entry *entries;
static size_t count;
static size_t capacity;

static void normalise_dll(const char *dll, char *out, size_t size) {
    size_t i = 0;
    int has_dot = 0;
    for (; dll[i] && i + 1 < size; i++) {
        out[i] = rc_ascii_lower(dll[i]);
        if (dll[i] == '.') has_dot = 1;
    }
    out[i] = '\0';
    if (!has_dot && i + 4 < size) memcpy(out + i, ".dll", 5);
}

static hle_entry *find(const char *ndll, const char *name, uint16_t ordinal) {
    for (size_t i = 0; i < count; i++) {
        hle_entry *e = &entries[i];
        if (strcmp(e->dll, ndll) != 0) continue;
        if (name && e->name && strcmp(e->name, name) == 0) return e;
        if (!name && ordinal && e->ordinal == ordinal) return e;
    }
    return NULL;
}

static void trap(rc_cpu *c) {
    const char *label = rc_dispatch_name(c->eip);
    rc_fatal("unimplemented import %s (return address 0x%08x)", label ? label : "?",
             rc_ld32(c->esp));
}

static hle_entry *add(const char *ndll, const char *name, uint16_t ordinal) {
    if (count >= RC_HLE_MAX_THUNKS) return NULL;
    if (count == capacity) {
        size_t new_cap = capacity ? capacity * 2 : 256;
        hle_entry *n = realloc(entries, new_cap * sizeof *n);
        if (!n) return NULL;
        entries = n;
        capacity = new_cap;
    }
    char label[160];
    if (name)
        snprintf(label, sizeof label, "%s!%s", ndll, name);
    else
        snprintf(label, sizeof label, "%s!#%u", ndll, (unsigned)ordinal);
    hle_entry *e = &entries[count];
    e->dll = rc_strdup(ndll);
    e->name = name ? rc_strdup(name) : NULL;
    e->label = rc_strdup(label);
    if (!e->dll || !e->label || (name && !e->name)) {
        free(e->dll);
        free(e->name);
        free(e->label);
        return NULL;
    }
    e->ordinal = ordinal;
    e->implemented = 0;
    e->thunk = RC_HLE_THUNK_BASE + (uint32_t)count * RC_HLE_THUNK_STRIDE;
    count++;
    return e;
}

uint32_t rc_hle_register(const char *dll, const char *name, uint16_t ordinal, rc_fn fn) {
    char ndll[64];
    normalise_dll(dll, ndll, sizeof ndll);
    hle_entry *e = find(ndll, name, ordinal);
    if (!e && name && ordinal) e = find(ndll, NULL, ordinal); /* was resolved by ordinal */
    if (!e) e = add(ndll, name, ordinal);
    if (!e || !fn) return 0;
    if (ordinal) e->ordinal = ordinal;
    e->implemented = 1;
    if (rc_dispatch_register(e->thunk, fn, e->label) != 0) return 0;
    return e->thunk;
}

static uint32_t resolve(const char *dll, const char *name, uint16_t ordinal) {
    char ndll[64];
    normalise_dll(dll, ndll, sizeof ndll);
    hle_entry *e = find(ndll, name, ordinal);
    if (e) return e->thunk;
    e = add(ndll, name, ordinal);
    if (!e || rc_dispatch_register(e->thunk, trap, e->label) != 0) {
        rc_fatal("HLE thunk table exhausted while resolving %s", dll);
    }
    return e->thunk;
}

uint32_t rc_hle_resolve(const char *dll, const char *name) { return resolve(dll, name, 0); }

uint32_t rc_hle_resolve_ordinal(const char *dll, uint16_t ordinal) {
    return resolve(dll, NULL, ordinal);
}

int rc_hle_is_implemented(const char *dll, const char *name) {
    char ndll[64];
    normalise_dll(dll, ndll, sizeof ndll);
    hle_entry *e = find(ndll, name, 0);
    return e && e->implemented;
}

size_t rc_hle_count(void) { return count; }

void rc_hle_reset(void) {
    for (size_t i = 0; i < count; i++) {
        free(entries[i].dll);
        free(entries[i].name);
        free(entries[i].label);
    }
    free(entries);
    entries = NULL;
    count = capacity = 0;
    /* The dispatch table points at our labels; drop it as well. */
    rc_dispatch_reset();
}
