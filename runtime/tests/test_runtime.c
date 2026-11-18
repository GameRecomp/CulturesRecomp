/* SPDX-License-Identifier: MIT */
#include "rc/cpu.h"
#include "rc/dispatch.h"
#include "rc/hle.h"
#include "rc/log.h"
#include "rc/mem.h"
#include "rc/pe.h"
#include "rc/vfs.h"
#include "rc_test.h"

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#define make_dir(p) mkdir(p, 0755)
#endif

static jmp_buf fatal_jmp;
static char fatal_msg[1024];

static void catch_fatal(const char *msg) {
    snprintf(fatal_msg, sizeof fatal_msg, "%s", msg);
    longjmp(fatal_jmp, 1);
}

static void test_mem(void) {
    CHECK(rc_mem_init() == 0);
    CHECK(rc_mem_init() == 0); /* idempotent */
    CHECK(!rc_mem_is_mapped(0x00400000));
    CHECK(rc_mem_map(0x00400000, 0x2000) == 0);
    CHECK(rc_mem_is_mapped(0x00400000));
    CHECK(rc_mem_is_mapped(0x00401fff));
    CHECK_EQ_U32(rc_ld32(0x00400000), 0); /* zero-filled */
    rc_st32(0x00400010, 0xdeadbeef);
    CHECK_EQ_U32(rc_ld32(0x00400010), 0xdeadbeef);
    CHECK_EQ_U32(rc_ld8(0x00400010), 0xef); /* little-endian */
    CHECK_EQ_U32(rc_ld16(0x00400012), 0xdead);
    rc_st32(0x00400ffe, 0x11223344); /* straddles a page boundary */
    CHECK_EQ_U32(rc_ld32(0x00400ffe), 0x11223344);
    CHECK(rc_mem_map(0x00400000, 0x1000) == 0); /* remap keeps contents */
    CHECK_EQ_U32(rc_ld32(0x00400010), 0xdeadbeef);
    CHECK(rc_mem_unmap(0x00400000, 0x2000) == 0);
    CHECK(!rc_mem_is_mapped(0x00400000));
    CHECK(rc_mem_map(0x00400000, 0x1000) == 0);
    CHECK_EQ_U32(rc_ld32(0x00400010), 0); /* contents gone after unmap */
    CHECK(rc_mem_map(0xfffff000u, 0x2000) != 0); /* leaves the guest space */
    CHECK(rc_mem_map(0x00500000, 0) != 0);
    CHECK(rc_mem_unmap(0x00400000, 0x1000) == 0);
}

static void test_cpu_stack(void) {
    rc_cpu c;
    CHECK(rc_mem_map(0x00100000, 0x10000) == 0);
    rc_cpu_init(&c, 0x00110000);
    CHECK_EQ_U32(c.fpu.cw, 0x027f);
    rc_push32(&c, 0xcafebabe);
    rc_push32(&c, 7);
    CHECK_EQ_U32(c.esp, 0x00110000 - 8);
    CHECK_EQ_U32(rc_pop32(&c), 7);
    CHECK_EQ_U32(rc_pop32(&c), 0xcafebabe);
    CHECK_EQ_U32(c.esp, 0x00110000);
}

static int calls;
static void fn_a(rc_cpu *c) {
    calls++;
    c->eax = c->eip;
}
static void fn_b(rc_cpu *c) { c->eax = 0xb; }

static void test_dispatch(void) {
    rc_dispatch_reset();
    rc_cpu c;
    rc_cpu_init(&c, 0x00110000);
    CHECK(rc_dispatch_register(0, fn_a, NULL) != 0);
    CHECK(rc_dispatch_register(0x401000, NULL, NULL) != 0);
    /* Enough entries to force several rehashes. */
    for (uint32_t i = 0; i < 20000; i++) CHECK(rc_dispatch_register(0x401000 + 16 * i, fn_a, "a") == 0);
    CHECK_EQ_U32(rc_dispatch_count(), 20000);
    CHECK(rc_dispatch_lookup(0x401000 + 16 * 19999) == fn_a);
    CHECK(rc_dispatch_lookup(0x401008) == NULL);
    rc_dispatch(&c, 0x401010);
    CHECK_EQ_U32(c.eax, 0x401010);
    CHECK_EQ_U32(c.eip, 0x401010);
    CHECK(rc_dispatch_register(0x401010, fn_b, "b") == 0); /* replace */
    CHECK_EQ_U32(rc_dispatch_count(), 20000);
    rc_dispatch(&c, 0x401010);
    CHECK_EQ_U32(c.eax, 0xb);
    CHECK(strcmp(rc_dispatch_name(0x401010), "b") == 0);

    rc_set_fatal_handler(catch_fatal);
    fatal_msg[0] = '\0';
    if (setjmp(fatal_jmp) == 0) rc_dispatch(&c, 0x12345678);
    CHECK(strstr(fatal_msg, "0x12345678") != NULL);
    rc_set_fatal_handler(NULL);
    rc_dispatch_reset();
    CHECK_EQ_U32(rc_dispatch_count(), 0);
}

static void hle_add(rc_cpu *c) { rc_hle_return(c, rc_arg(c, 0) + rc_arg(c, 1), 2); }

static void test_hle(void) {
    rc_hle_reset();
    rc_cpu c;
    rc_cpu_init(&c, 0x00110000);
    uint32_t t = rc_hle_register("TEST.dll", "Add", 3, hle_add);
    CHECK_EQ_U32(t, RC_HLE_THUNK_BASE);
    CHECK_EQ_U32(rc_hle_resolve("test.DLL", "Add"), t);   /* DLL name case-insensitive */
    CHECK_EQ_U32(rc_hle_resolve("test", "Add"), t);       /* ".dll" implied */
    CHECK_EQ_U32(rc_hle_resolve_ordinal("TEST.dll", 3), t);
    CHECK(rc_hle_is_implemented("TEST.dll", "Add"));

    /* stdcall: push args right to left, then the return address. */
    rc_push32(&c, 40);
    rc_push32(&c, 2);
    rc_push32(&c, 0x00401234);
    rc_dispatch(&c, t);
    CHECK_EQ_U32(c.eax, 42);
    CHECK_EQ_U32(c.esp, 0x00110000);

    /* Unknown imports resolve to distinct trap thunks that fail only when called. */
    uint32_t missing = rc_hle_resolve("KERNEL32.dll", "NotThere");
    CHECK(missing != 0 && missing != t);
    CHECK_EQ_U32(rc_hle_resolve("kernel32.dll", "NotThere"), missing);
    CHECK(!rc_hle_is_implemented("kernel32.dll", "NotThere"));
    uint32_t by_ord = rc_hle_resolve_ordinal("DSOUND.dll", 1);
    CHECK(strcmp(rc_dispatch_name(by_ord), "dsound.dll!#1") == 0);

    rc_set_fatal_handler(catch_fatal);
    fatal_msg[0] = '\0';
    rc_push32(&c, 0x00405678);
    if (setjmp(fatal_jmp) == 0) rc_dispatch(&c, missing);
    CHECK(strstr(fatal_msg, "kernel32.dll!NotThere") != NULL);
    CHECK(strstr(fatal_msg, "0x00405678") != NULL);
    rc_set_fatal_handler(NULL);

    /* Registering later upgrades the trap in place: the IAT value stays valid. */
    CHECK_EQ_U32(rc_hle_register("kernel32.dll", "NotThere", 0, hle_add), missing);
    CHECK(rc_hle_is_implemented("KERNEL32.DLL", "NotThere"));
    CHECK_EQ_U32(rc_hle_register("dsound.dll", "DirectSoundCreate", 1, hle_add), by_ord);
    CHECK_EQ_U32(rc_hle_count(), 3);
    rc_hle_reset();
    CHECK_EQ_U32(rc_hle_count(), 0);
}


static void put16(uint8_t *b, size_t o, uint16_t v) {
    b[o] = (uint8_t)v;
    b[o + 1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *b, size_t o, uint32_t v) {
    put16(b, o, (uint16_t)v);
    put16(b, o + 2, (uint16_t)(v >> 16));
}

/* Synthetic PE32: .text (ret) at 0x1000, .idata at 0x2000 importing
 * KERNEL32.dll!GetTickCount by name and DSOUND.dll #1 by ordinal. */
static uint8_t pe_file[0x600];

static void build_pe(void) {
    uint8_t *f = pe_file;
    memset(f, 0, sizeof pe_file);
    f[0] = 'M';
    f[1] = 'Z';
    put32(f, 0x3c, 0x80);
    memcpy(f + 0x80, "PE\0\0", 4);
    const size_t coff = 0x84, opt = coff + 20;
    put16(f, coff, 0x014c);
    put16(f, coff + 2, 2);
    put16(f, coff + 16, 224);
    put16(f, coff + 18, 0x010f);
    put16(f, opt, 0x10b);
    put32(f, opt + 16, 0x1000);     /* entry */
    put32(f, opt + 28, 0x00400000); /* image base */
    put32(f, opt + 56, 0x3000);     /* size of image */
    put32(f, opt + 60, 0x200);      /* size of headers */
    put32(f, opt + 92, 16);
    put32(f, opt + 104, 0x2000); /* import directory */
    put32(f, opt + 108, 0x3c);
    const size_t sec = opt + 224;
    memcpy(f + sec, ".text", 5);
    put32(f, sec + 8, 0x100);
    put32(f, sec + 12, 0x1000);
    put32(f, sec + 16, 0x200);
    put32(f, sec + 20, 0x200);
    memcpy(f + sec + 40, ".idata", 6);
    put32(f, sec + 48, 0x200);
    put32(f, sec + 52, 0x2000);
    put32(f, sec + 56, 0x200);
    put32(f, sec + 60, 0x400);
    f[0x200] = 0xc3;
#define R(rva) (0x400 + (rva) - 0x2000)
    put32(f, R(0x2000), 0x2040); /* KERNEL32: ILT, name, IAT */
    put32(f, R(0x200c), 0x2070);
    put32(f, R(0x2010), 0x2050);
    put32(f, R(0x2014), 0x2090); /* DSOUND */
    put32(f, R(0x2020), 0x2080);
    put32(f, R(0x2024), 0x2098);
    put32(f, R(0x2040), 0x20a0);
    put32(f, R(0x2050), 0x20a0);
    memcpy(f + R(0x2070), "KERNEL32.dll", 13);
    memcpy(f + R(0x2080), "DSOUND.dll", 11);
    put32(f, R(0x2090), 0x80000001u);
    put32(f, R(0x2098), 0x80000001u);
    memcpy(f + R(0x20a2), "GetTickCount", 13);
#undef R
}

static void fake_ticks(rc_cpu *c) { rc_hle_return(c, 1234, 0); }

static void test_pe(void) {
    rc_hle_reset();
    rc_hle_register("kernel32.dll", "GetTickCount", 0, fake_ticks);
    build_pe();
    rc_pe_image img;
    char err[256] = "";
    CHECK(rc_pe_load(pe_file, sizeof pe_file, &img, err, sizeof err) == 0);
    if (err[0]) fprintf(stderr, "pe: %s\n", err);
    CHECK_EQ_U32(img.image_base, 0x00400000);
    CHECK_EQ_U32(img.entry, 0x00401000);
    CHECK_EQ_U32(img.section_count, 2);
    CHECK_EQ_U32(img.import_dlls, 2);
    CHECK_EQ_U32(img.imports, 2);
    CHECK_EQ_U32(img.imports_missing, 1); /* DSOUND #1 has no implementation yet */
    CHECK(!img.is_dll);
    CHECK_EQ_U32(rc_ld8(0x00401000), 0xc3);
    CHECK_EQ_U32(rc_ld16(0x00400000), 0x5a4d); /* headers mapped */
    uint32_t t = rc_ld32(0x00402050);
    CHECK_EQ_U32(t, rc_hle_resolve("KERNEL32.dll", "GetTickCount"));
    CHECK_EQ_U32(rc_ld32(0x00402098), rc_hle_resolve_ordinal("dsound", 1));
    CHECK(strcmp(rc_hle_thunk_label(rc_ld32(0x00402098)), "dsound.dll!#1") == 0);

    /* The lifted `call [0x402050]` reaches the HLE function through the thunk. */
    rc_cpu c;
    rc_cpu_init(&c, 0x00110000);
    rc_push32(&c, 0x00401000);
    rc_dispatch(&c, rc_ld32(0x00402050));
    CHECK_EQ_U32(c.eax, 1234);
    CHECK_EQ_U32(c.esp, 0x00110000);

    CHECK(rc_pe_load(pe_file, sizeof pe_file, &img, err, sizeof err) != 0); /* range in use */
    CHECK(strstr(err, "already in use") != NULL);
    CHECK(rc_mem_unmap(0x00400000, 0x3000) == 0);
    CHECK(rc_pe_load(pe_file, 0x100, &img, err, sizeof err) != 0); /* truncated */
    pe_file[0x84] = 0x64; /* machine: AMD64 */
    CHECK(rc_pe_load(pe_file, sizeof pe_file, &img, err, sizeof err) != 0);
    CHECK(strstr(err, "i386") != NULL);
    rc_hle_reset();
}

static void touch(const char *path) {
    FILE *f = fopen(path, "wb");
    if (f) fclose(f);
}

static const char *vfs_root = "vfs_test_root";

static void test_vfs(void) {
    const char *root = vfs_root;
    char p[1024], out[1024];
    make_dir(root);
    snprintf(p, sizeof p, "%s/data_fmv", root);
    make_dir(p);
    snprintf(p, sizeof p, "%s/data_fmv/intro.bik", root);
    touch(p);
    snprintf(p, sizeof p, "%s/Cultures.exe", root);
    touch(p);

    CHECK(rc_vfs_resolve(root, "Cultures.exe", out, sizeof out) == 0);
    CHECK(rc_vfs_resolve(root, "CULTURES.EXE", out, sizeof out) == 0);
    CHECK(rc_vfs_resolve(root, "Data_FMV\\Intro.bik", out, sizeof out) == 0);
    snprintf(p, sizeof p, "%s/data_fmv/intro.bik", root);
    CHECK(strcmp(out, p) == 0);
    CHECK(rc_vfs_resolve(root, ".\\data_fmv\\\\intro.bik", out, sizeof out) == 0);
    CHECK(rc_vfs_resolve(root, "Data_FMV\\new.sav", out, sizeof out) == 1); /* creatable */
    snprintf(p, sizeof p, "%s/data_fmv/new.sav", root);
    CHECK(strcmp(out, p) == 0);
    CHECK(rc_vfs_resolve(root, "missing\\file", out, sizeof out) == -1);
    CHECK(rc_vfs_resolve(root, "..\\escape", out, sizeof out) == -1);
    CHECK(rc_vfs_resolve(root, "Cultures.exe", out, 8) == -1); /* buffer too small */
}

int main(int argc, char **argv) {
    if (argc > 1) vfs_root = argv[1];
    RUN(test_mem);
    RUN(test_cpu_stack);
    RUN(test_dispatch);
    RUN(test_hle);
    RUN(test_pe);
    RUN(test_vfs);
    rc_mem_shutdown();
    return TEST_EXIT();
}
