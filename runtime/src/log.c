/* SPDX-License-Identifier: MIT */
#include "rc/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int level_initialised;
static rc_log_level current_level = RC_LOG_INFO;
static rc_fatal_handler fatal_handler;

static const char *const level_names[] = {"error", "warn", "info", "debug", "trace"};

static void init_level(void) {
    if (level_initialised) return;
    level_initialised = 1;
    const char *env = getenv("RC_LOG");
    if (!env) return;
    for (int i = 0; i <= RC_LOG_TRACE; i++) {
        if (strcmp(env, level_names[i]) == 0) current_level = (rc_log_level)i;
    }
}

void rc_log_set_level(rc_log_level level) {
    level_initialised = 1;
    current_level = level;
}

rc_log_level rc_log_get_level(void) {
    init_level();
    return current_level;
}

void rc_log(rc_log_level level, const char *fmt, ...) {
    init_level();
    if (level > current_level) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[rc] %s: ", level_names[level]);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

void rc_set_fatal_handler(rc_fatal_handler handler) { fatal_handler = handler; }

void rc_fatal(const char *fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    fprintf(stderr, "[rc] fatal: %s\n", msg);
    fflush(stderr);
    if (fatal_handler) fatal_handler(msg);
    abort();
}
