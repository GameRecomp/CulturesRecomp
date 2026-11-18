/* SPDX-License-Identifier: MIT */
#include "rc/pe.h"
#include "rc/hle.h"
#include "rc/mem.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(char *err, size_t size, const char *fmt, ...) {
    if (err && size) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err, size, fmt, ap);
        va_end(ap);
    }
    return -1;
}

static int rd16(const uint8_t *f, size_t size, size_t o, uint16_t *v) {
    if (o > size || size - o < 2) return -1;
    *v = (uint16_t)(f[o] | f[o + 1] << 8);
    return 0;
}

static int rd32(const uint8_t *f, size_t size, size_t o, uint32_t *v) {
    if (o > size || size - o < 4) return -1;
    *v = (uint32_t)f[o] | (uint32_t)f[o + 1] << 8 | (uint32_t)f[o + 2] << 16 |
         (uint32_t)f[o + 3] << 24;
    return 0;
}

/* NUL-terminated string in guest memory inside [lo, hi); copies at most n-1 bytes. */
static int guest_str(uint32_t va, uint32_t lo, uint32_t hi, char *out, size_t n) {
    size_t i = 0;
    for (; va + i < hi && va + i >= lo; i++) {
        char c = (char)rc_ld8(va + (uint32_t)i);
        if (i + 1 >= n) return -1;
        out[i] = c;
        if (c == '\0') return 0;
    }
    return -1;
}

int rc_pe_load(const uint8_t *f, size_t size, rc_pe_image *out, char *err, size_t err_size) {
    memset(out, 0, sizeof *out);
    if (!rc_mem_base) return fail(err, err_size, "guest memory not initialised");
    uint32_t pe;
    if (size < 0x40 || f[0] != 'M' || f[1] != 'Z' || rd32(f, size, 0x3c, &pe) != 0 ||
        pe > size - 4 || memcmp(f + pe, "PE\0\0", 4) != 0)
        return fail(err, err_size, "not a PE file");

    uint16_t machine, nsec, opt_size, characteristics, magic;
    const size_t coff = (size_t)pe + 4;
    if (rd16(f, size, coff, &machine) || rd16(f, size, coff + 2, &nsec) ||
        rd16(f, size, coff + 16, &opt_size) || rd16(f, size, coff + 18, &characteristics))
        return fail(err, err_size, "truncated COFF header");
    if (machine != 0x014c) return fail(err, err_size, "machine 0x%04x is not i386", machine);
    const size_t opt = coff + 20;
    if (rd16(f, size, opt, &magic) || magic != 0x10b)
        return fail(err, err_size, "not a PE32 optional header");

    uint32_t entry_rva, base, image_size, headers_size, ndirs, imp_rva = 0, imp_size = 0;
    if (rd32(f, size, opt + 16, &entry_rva) || rd32(f, size, opt + 28, &base) ||
        rd32(f, size, opt + 56, &image_size) || rd32(f, size, opt + 60, &headers_size) ||
        rd32(f, size, opt + 92, &ndirs))
        return fail(err, err_size, "truncated optional header");
    if (ndirs > 1 && (rd32(f, size, opt + 104, &imp_rva) || rd32(f, size, opt + 108, &imp_size)))
        return fail(err, err_size, "truncated data directories");
    if (image_size == 0 || (uint64_t)base + image_size > 0xF0000000u || base < 0x10000)
        return fail(err, err_size, "image range 0x%08x+0x%x not loadable", base, image_size);
    for (uint32_t a = base; a < base + image_size; a += RC_PAGE_SIZE) {
        if (rc_mem_is_mapped(a))
            return fail(err, err_size, "image range at 0x%08x already in use (relocation "
                                       "is not supported)", a);
    }
    if (rc_mem_map(base, image_size) != 0)
        return fail(err, err_size, "cannot map 0x%08x+0x%x", base, image_size);

    /* Headers, then each section's raw data. The rest of the image stays zero. */
    size_t hcopy = headers_size < size ? headers_size : size;
    if (hcopy > image_size) hcopy = image_size;
    memcpy(rc_ptr(base), f, hcopy);
    const size_t sec = opt + opt_size;
    for (uint16_t i = 0; i < nsec; i++) {
        uint32_t vsize, va, raw_size, raw_off;
        const size_t s = sec + 40u * i;
        if (rd32(f, size, s + 8, &vsize) || rd32(f, size, s + 12, &va) ||
            rd32(f, size, s + 16, &raw_size) || rd32(f, size, s + 20, &raw_off))
            return fail(err, err_size, "truncated section table");
        uint32_t span = vsize ? vsize : raw_size;
        uint32_t n = raw_size < span ? raw_size : span;
        if ((uint64_t)va + span > image_size)
            return fail(err, err_size, "section %u outside the image", (unsigned)i);
        if (n) {
            if ((uint64_t)raw_off + n > size) n = raw_off < size ? (uint32_t)(size - raw_off) : 0;
            memcpy(rc_ptr(base + va), f + raw_off, n);
        }
    }

    out->image_base = base;
    out->size_of_image = image_size;
    out->entry = entry_rva ? base + entry_rva : 0;
    out->section_count = nsec;
    out->is_dll = (characteristics & 0x2000) != 0;

    /* Imports: walk the descriptors in guest memory and fill the IAT. */
    if (imp_rva && imp_size) {
        const uint32_t lo = base, hi = base + image_size;
        for (uint32_t d = base + imp_rva;; d += 20) {
            if (d < lo || d + 20 > hi) return fail(err, err_size, "import directory outside image");
            uint32_t ilt = rc_ld32(d), name = rc_ld32(d + 12), iat = rc_ld32(d + 16);
            if (!name && !iat) break;
            char dll[64];
            if (guest_str(base + name, lo, hi, dll, sizeof dll) != 0)
                return fail(err, err_size, "bad import DLL name");
            out->import_dlls++;
            uint32_t lookup = base + (ilt ? ilt : iat);
            for (uint32_t j = 0;; j++) {
                uint32_t slot = lookup + 4 * j, iat_slot = base + iat + 4 * j;
                if (slot + 4 > hi || iat_slot + 4 > hi)
                    return fail(err, err_size, "import table of %s outside image", dll);
                uint32_t v = rc_ld32(slot);
                if (!v) break;
                uint32_t thunk;
                if (v & 0x80000000u) {
                    thunk = rc_hle_resolve_ordinal(dll, (uint16_t)(v & 0xffff));
                } else {
                    char fn[128];
                    if (guest_str(base + v + 2, lo, hi, fn, sizeof fn) != 0)
                        return fail(err, err_size, "bad import name in %s", dll);
                    thunk = rc_hle_resolve(dll, fn);
                }
                rc_st32(iat_slot, thunk);
                out->imports++;
                if (!rc_hle_thunk_is_implemented(thunk)) out->imports_missing++;
            }
        }
    }
    return 0;
}

int rc_pe_load_file(const char *path, rc_pe_image *out, char *err, size_t err_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return fail(err, err_size, "cannot open %s", path);
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return fail(err, err_size, "cannot seek %s", path);
    }
    long len = ftell(fp);
    if (len <= 0 || len > (64L << 20) || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return fail(err, err_size, "bad size of %s", path);
    }
    uint8_t *buf = malloc((size_t)len);
    if (!buf) {
        fclose(fp);
        return fail(err, err_size, "out of memory");
    }
    size_t got = fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    int rc = got == (size_t)len ? rc_pe_load(buf, got, out, err, err_size)
                                : fail(err, err_size, "short read of %s", path);
    free(buf);
    return rc;
}
