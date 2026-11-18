/* SPDX-License-Identifier: MIT */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "rc/vfs.h"
#include "strutil.h"

#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

static int append(char *out, size_t size, size_t *len, const char *s, size_t n) {
    if (*len + n + 1 > size) return -1;
    memcpy(out + *len, s, n);
    *len += n;
    out[*len] = '\0';
    return 0;
}

static int exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* Find an entry of directory `dir` whose name equals `name` ignoring ASCII case. */
static int find_ci(const char *dir, const char *name, char *found, size_t size) {
#if defined(_WIN32)
    (void)dir;
    (void)name;
    (void)found;
    (void)size;
    return -1; /* NTFS is case-insensitive; exists() already matched */
#else
    DIR *d = opendir(dir);
    if (!d) return -1;
    int rc = -1;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (rc_ascii_ieq(de->d_name, name) && strlen(de->d_name) < size) {
            strcpy(found, de->d_name);
            rc = 0;
            break;
        }
    }
    closedir(d);
    return rc;
#endif
}

int rc_vfs_resolve(const char *root, const char *guest_path, char *out, size_t out_size) {
    size_t len = 0;
    if (!out_size) return -1;
    out[0] = '\0';
    if (append(out, out_size, &len, root, strlen(root)) != 0) return -1;

    const char *p = guest_path;
    while (*p) {
        while (*p == '\\' || *p == '/') p++;
        const char *start = p;
        while (*p && *p != '\\' && *p != '/') p++;
        size_t n = (size_t)(p - start);
        if (n == 0 || (n == 1 && start[0] == '.')) continue;
        if (n == 2 && start[0] == '.' && start[1] == '.') return -1;

        char comp[260];
        if (n >= sizeof comp) return -1;
        memcpy(comp, start, n);
        comp[n] = '\0';
        int last = 1;
        for (const char *q = p; *q; q++) {
            if (*q != '\\' && *q != '/') {
                last = 0;
                break;
            }
        }

        size_t dir_len = len;
        if (append(out, out_size, &len, "/", 1) != 0 || append(out, out_size, &len, comp, n) != 0)
            return -1;
        if (exists(out)) continue;

        char found[260];
        out[dir_len] = '\0';
        int ok = find_ci(out, comp, found, sizeof found);
        len = dir_len;
        if (ok == 0) {
            if (append(out, out_size, &len, "/", 1) != 0 ||
                append(out, out_size, &len, found, strlen(found)) != 0)
                return -1;
            continue;
        }
        if (!last) return -1;
        if (append(out, out_size, &len, "/", 1) != 0 || append(out, out_size, &len, comp, n) != 0)
            return -1;
        return 1;
    }
    return exists(out) ? 0 : -1;
}
