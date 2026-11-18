/* SPDX-License-Identifier: MIT */
/* Headless backend: no window, no input. Used for tests and CI. */
#include "rc/platform.h"

#include <time.h>

static int initialised;

const char *rc_platform_backend(void) { return "null"; }

int rc_platform_init(const char *title, int w, int h) {
    (void)title;
    if (w <= 0 || h <= 0) return -1;
    initialised = 1;
    return 0;
}

void rc_platform_shutdown(void) { initialised = 0; }

int rc_platform_poll(rc_event *ev) {
    ev->type = RC_EV_NONE;
    return 0;
}

void rc_platform_present16(const void *pixels, int w, int h, int pitch, rc_pixel_format fmt) {
    (void)pixels;
    (void)w;
    (void)h;
    (void)pitch;
    (void)fmt;
}

uint64_t rc_platform_ticks_ms(void) {
    static uint64_t start;
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    uint64_t now = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    if (!start) start = now;
    return now >= start ? now - start : 0;
}
