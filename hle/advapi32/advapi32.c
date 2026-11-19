/* SPDX-License-Identifier: MIT */
/*
 * ADVAPI32: the registry, and the user name.
 *
 * The registry is the program's own, kept in registry.txt in the user directory: what the
 * game writes is there on the next start, nothing of the host's is visible. A key is a path
 * below a root ("HKLM\Software\..."), compared without case as Windows does; values are
 * strings, DWORDs or binary. A key that was never created does not exist, as on a clean
 * install, so a game falls back to its defaults (or creates it).
 *
 * The file is plain text, one line per value:  KEY<TAB>NAME<TAB>TYPE<TAB>HEX.
 */
#include "advapi32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rc/hostfs.h"
#include "rc/log.h"
#include "rc/process.h"

#define ADV "ADVAPI32.dll"

#define ERROR_SUCCESS 0u
#define ERROR_FILE_NOT_FOUND 2u
#define ERROR_INVALID_HANDLE 6u
#define ERROR_MORE_DATA 234u
#define ERROR_NO_MORE_ITEMS 259u
#define REG_CREATED_NEW_KEY 1u
#define REG_OPENED_EXISTING_KEY 2u

#define KEYS 256
#define VALUES 1024
#define HANDLES 64
#define HANDLE_BASE 0x00c0de00u

typedef struct value {
    int key;
    char name[64];
    uint32_t type;
    uint32_t size;
    uint8_t *data;
} value;

static char keys[KEYS][256];
static int nkeys;
static value values[VALUES];
static int nvalues;
static int handles[HANDLES]; /* key index + 1, 0 = free */
static bool loaded;

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? (char)(*a - 'A' + 'a') : *a;
        char y = *b >= 'A' && *b <= 'Z' ? (char)(*b - 'A' + 'a') : *b;
        if (x != y) {
            return 0;
        }
    }
    return *a == *b;
}

static int find_key(const char *path)
{
    for (int i = 0; i < nkeys; i++) {
        if (ieq(keys[i], path)) {
            return i;
        }
    }
    return -1;
}

/* The key and, with `create`, every key above it. */
static int add_key(const char *path)
{
    int k = find_key(path);
    if (k >= 0 || nkeys == KEYS) {
        return k;
    }
    char parent[256];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '\\');
    if (slash && strchr(parent, '\\') != slash) {
        *slash = 0;
        add_key(parent);
    }
    snprintf(keys[nkeys], sizeof keys[nkeys], "%s", path);
    return nkeys++;
}

static value *find_value(int key, const char *name)
{
    for (int i = 0; i < nvalues; i++) {
        if (values[i].key == key && ieq(values[i].name, name)) {
            return &values[i];
        }
    }
    return NULL;
}

static void set_value(int key, const char *name, uint32_t type, const uint8_t *data, uint32_t size)
{
    value *v = find_value(key, name);
    if (!v) {
        if (nvalues == VALUES) {
            return;
        }
        v = &values[nvalues++];
        v->key = key;
        snprintf(v->name, sizeof v->name, "%s", name);
        v->data = NULL;
    }
    free(v->data);
    v->type = type;
    v->size = size;
    v->data = malloc(size ? size : 1);
    if (v->data && size) {
        memcpy(v->data, data, size);
    }
}

static bool file_path(char *out)
{
    return rc_cfg && rc_cfg->user_dir && rc_host_resolve(rc_cfg->user_dir, "registry.txt", true, out);
}

static void load(void)
{
    if (loaded) {
        return;
    }
    loaded = true;
    char path[RC_PATH_MAX];
    FILE *f = file_path(path) ? fopen(path, "r") : NULL;
    if (!f) {
        return;
    }
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *k = line, *n = strchr(k, '\t');
        char *t = n ? strchr(n + 1, '\t') : NULL;
        char *h = t ? strchr(t + 1, '\t') : NULL;
        if (!h) {
            continue;
        }
        *n++ = 0;
        *t++ = 0;
        *h++ = 0;
        int key = add_key(k);
        if (key < 0) {
            continue;
        }
        if (*n == 0 && *t == '-') {
            continue; /* an empty key */
        }
        uint8_t data[2048];
        uint32_t size = 0;
        while (h[0] && h[1] && h[0] != '\n' && size < sizeof data) {
            unsigned b;
            if (sscanf(h, "%2x", &b) != 1) {
                break;
            }
            data[size++] = (uint8_t)b;
            h += 2;
        }
        set_value(key, n, (uint32_t)strtoul(t, NULL, 10), data, size);
    }
    fclose(f);
}

static void save(void)
{
    char path[RC_PATH_MAX];
    FILE *f = file_path(path) ? fopen(path, "w") : NULL;
    if (!f) {
        return;
    }
    for (int k = 0; k < nkeys; k++) {
        bool any = false;
        for (int i = 0; i < nvalues; i++) {
            if (values[i].key != k) {
                continue;
            }
            any = true;
            fprintf(f, "%s\t%s\t%u\t", keys[k], values[i].name, values[i].type);
            for (uint32_t j = 0; j < values[i].size; j++) {
                fprintf(f, "%02x", values[i].data[j]);
            }
            fputc('\n', f);
        }
        if (!any) {
            fprintf(f, "%s\t\t-\t\n", keys[k]);
        }
    }
    fclose(f);
}

/* The path of `sub` below the handle or root `h`; false for an unknown handle. */
static bool path_of(uint32_t h, const char *sub, char *out, size_t cap)
{
    const char *root = NULL;
    switch (h) {
    case 0x80000000u: root = "HKCR"; break;
    case 0x80000001u: root = "HKCU"; break;
    case 0x80000002u: root = "HKLM"; break;
    case 0x80000003u: root = "HKU"; break;
    case 0x80000005u: root = "HKCC"; break;
    default:
        if (h >= HANDLE_BASE && h < HANDLE_BASE + HANDLES && handles[h - HANDLE_BASE]) {
            root = keys[handles[h - HANDLE_BASE] - 1];
        }
        break;
    }
    if (!root) {
        return false;
    }
    if (sub && *sub) {
        snprintf(out, cap, "%s\\%s", root, sub);
    } else {
        snprintf(out, cap, "%s", root);
    }
    size_t n = strlen(out);
    while (n && out[n - 1] == '\\') {
        out[--n] = 0;
    }
    return true;
}

static uint32_t new_handle(int key)
{
    for (int i = 0; i < HANDLES; i++) {
        if (!handles[i]) {
            handles[i] = key + 1;
            return HANDLE_BASE + (uint32_t)i;
        }
    }
    return 0;
}

static int key_of(uint32_t h)
{
    char path[256];
    return path_of(h, NULL, path, sizeof path) ? find_key(path) : -1;
}

/* RegOpenKeyExA(key, sub, options, sam, result) */
static void k_RegOpenKeyExA(rc_cpu *c)
{
    load();
    char path[256];
    if (!path_of(RC_ARG(c, 0), rc_str(RC_ARG(c, 1)), path, sizeof path)) {
        RC_RETURN(c, ERROR_INVALID_HANDLE, 5);
    }
    int k = find_key(path);
    if (rc_trace("registry")) {
        rc_log("[registry] open %s: %s", path, k < 0 ? "not found" : "ok");
    }
    if (k < 0) {
        RC_RETURN(c, ERROR_FILE_NOT_FOUND, 5);
    }
    rc_st32(RC_ARG(c, 4), new_handle(k));
    RC_RETURN(c, ERROR_SUCCESS, 5);
}

/* RegCreateKeyExA(key, sub, reserved, class, options, sam, security, result, disposition) */
static void k_RegCreateKeyExA(rc_cpu *c)
{
    load();
    char path[256];
    if (!path_of(RC_ARG(c, 0), rc_str(RC_ARG(c, 1)), path, sizeof path)) {
        RC_RETURN(c, ERROR_INVALID_HANDLE, 9);
    }
    bool existed = find_key(path) >= 0;
    int k = add_key(path);
    if (k < 0) {
        RC_RETURN(c, ERROR_FILE_NOT_FOUND, 9);
    }
    if (!existed) {
        save();
    }
    rc_st32(RC_ARG(c, 7), new_handle(k));
    if (RC_ARG(c, 8)) {
        rc_st32(RC_ARG(c, 8), existed ? REG_OPENED_EXISTING_KEY : REG_CREATED_NEW_KEY);
    }
    RC_RETURN(c, ERROR_SUCCESS, 9);
}

static void k_RegCloseKey(rc_cpu *c)
{
    uint32_t h = RC_ARG(c, 0);
    if (h >= HANDLE_BASE && h < HANDLE_BASE + HANDLES) {
        handles[h - HANDLE_BASE] = 0;
    }
    RC_RETURN(c, ERROR_SUCCESS, 1);
}

/* RegQueryValueExA(key, name, reserved, type, data, size) */
static void k_RegQueryValueExA(rc_cpu *c)
{
    load();
    int k = key_of(RC_ARG(c, 0));
    if (k < 0) {
        RC_RETURN(c, ERROR_INVALID_HANDLE, 6);
    }
    const char *name = rc_str(RC_ARG(c, 1));
    value *v = find_value(k, name ? name : "");
    if (rc_trace("registry")) {
        rc_log("[registry] query %s \\ %s: %s", keys[k], name ? name : "", v ? "ok" : "not found");
    }
    if (!v) {
        RC_RETURN(c, ERROR_FILE_NOT_FOUND, 6);
    }
    uint32_t ptype = RC_ARG(c, 3), data = RC_ARG(c, 4), psize = RC_ARG(c, 5);
    if (ptype) {
        rc_st32(ptype, v->type);
    }
    uint32_t r = ERROR_SUCCESS;
    if (psize) {
        uint32_t cap = rc_ld32(psize);
        if (data && cap < v->size) {
            r = ERROR_MORE_DATA;
        } else if (data) {
            memcpy(rc_ptr(data), v->data, v->size);
        }
        rc_st32(psize, v->size);
    }
    RC_RETURN(c, r, 6);
}

/* RegSetValueExA(key, name, reserved, type, data, size) */
static void k_RegSetValueExA(rc_cpu *c)
{
    load();
    int k = key_of(RC_ARG(c, 0));
    if (k < 0) {
        RC_RETURN(c, ERROR_INVALID_HANDLE, 6);
    }
    const char *name = rc_str(RC_ARG(c, 1));
    uint32_t size = RC_ARG(c, 5);
    set_value(k, name ? name : "", RC_ARG(c, 3), RC_ARG(c, 4) ? (const uint8_t *)rc_ptr(RC_ARG(c, 4)) : NULL,
              RC_ARG(c, 4) ? size : 0);
    save();
    RC_RETURN(c, ERROR_SUCCESS, 6);
}

/* RegDeleteValueA(key, name) */
static void k_RegDeleteValueA(rc_cpu *c)
{
    load();
    int k = key_of(RC_ARG(c, 0));
    const char *name = rc_str(RC_ARG(c, 1));
    value *v = k >= 0 ? find_value(k, name ? name : "") : NULL;
    if (!v) {
        RC_RETURN(c, ERROR_FILE_NOT_FOUND, 2);
    }
    free(v->data);
    *v = values[--nvalues];
    save();
    RC_RETURN(c, ERROR_SUCCESS, 2);
}

/* GetUserNameA(buffer, size) */
static void k_GetUserNameA(rc_cpu *c)
{
    const char *user = "Player";
    uint32_t buf = RC_ARG(c, 0), psize = RC_ARG(c, 1);
    uint32_t need = (uint32_t)strlen(user) + 1;
    if (!psize || rc_ld32(psize) < need) {
        if (psize) {
            rc_st32(psize, need);
        }
        RC_RETURN(c, 0, 2);
    }
    rc_put_str(buf, need, user);
    rc_st32(psize, need);
    RC_RETURN(c, 1, 2);
}

const rc_import_def rc_advapi32_defs[] = {
    {ADV, "RegOpenKeyExA", k_RegOpenKeyExA},
    {ADV, "RegCreateKeyExA", k_RegCreateKeyExA},
    {ADV, "RegCloseKey", k_RegCloseKey},
    {ADV, "RegQueryValueExA", k_RegQueryValueExA},
    {ADV, "RegSetValueExA", k_RegSetValueExA},
    {ADV, "RegDeleteValueA", k_RegDeleteValueA},
    {ADV, "GetUserNameA", k_GetUserNameA},
};
const uint32_t rc_advapi32_count = sizeof rc_advapi32_defs / sizeof rc_advapi32_defs[0];
