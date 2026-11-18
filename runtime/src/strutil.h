/* SPDX-License-Identifier: MIT */
/* Small string helpers that plain C11 lacks. Internal to the runtime. */
#ifndef RC_STRUTIL_H
#define RC_STRUTIL_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static inline char rc_ascii_lower(char ch) {
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
}

static inline int rc_ascii_ieq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        if (rc_ascii_lower(*a) != rc_ascii_lower(*b)) return 0;
    }
    return *a == *b;
}

static inline char *rc_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

#endif
