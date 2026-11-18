/* SPDX-License-Identifier: MIT */
/* PE32 (i386) loader: maps an executable or DLL into guest memory at its preferred base
 * and points every IAT slot at an HLE thunk (rc_hle_resolve). Lifted code then runs
 * against exactly the memory layout the original program expects. */
#ifndef RC_PE_H
#define RC_PE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rc_pe_image {
    uint32_t image_base;
    uint32_t size_of_image;
    uint32_t entry;          /* guest VA of the entry point (0 if none) */
    uint32_t section_count;
    uint32_t import_dlls;
    uint32_t imports;        /* functions resolved into the IAT */
    uint32_t imports_missing; /* of those, resolved to trap thunks (not implemented yet) */
    int is_dll;
} rc_pe_image;

/* Load a PE image from memory. Requires rc_mem_init(). On failure returns -1 and writes a
 * message to err. The image range must be free; relocation to another base is not
 * supported (none of the targets has relocations). */
int rc_pe_load(const uint8_t *file, size_t size, rc_pe_image *out, char *err, size_t err_size);

/* Read a file and rc_pe_load() it. */
int rc_pe_load_file(const char *path, rc_pe_image *out, char *err, size_t err_size);

#ifdef __cplusplus
}
#endif
#endif
