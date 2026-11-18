/* SPDX-License-Identifier: MIT */
/* Runs against the compiled backend. For SDL3 the test sets SDL_VIDEO_DRIVER=dummy. */
#include "rc/platform.h"
#include "rc_test.h"

#include <stdint.h>
#include <string.h>

static uint16_t frame[480][640];

static void test_backend(void) {
    const char *b = rc_platform_backend();
    CHECK(strcmp(b, "null") == 0 || strcmp(b, "sdl3") == 0);
    CHECK(rc_platform_init("test", 0, 480) != 0);
    CHECK(rc_platform_init("test", 640, 480) == 0);
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++) frame[y][x] = (uint16_t)((x >> 1) << 11 | y >> 3);
    rc_platform_present16(frame, 640, 480, 640 * 2, RC_PIXFMT_RGB565);
    rc_platform_present16(frame, 320, 240, 640 * 2, RC_PIXFMT_XRGB1555); /* recreate texture */
    rc_event ev;
    int n = 0;
    while (rc_platform_poll(&ev) && n < 1000) n++;
    CHECK(n < 1000);
    uint64_t t0 = rc_platform_ticks_ms();
    uint64_t t1 = rc_platform_ticks_ms();
    CHECK(t1 >= t0);
    rc_platform_shutdown();
}

int main(void) {
    printf("backend: %s\n", rc_platform_backend());
    RUN(test_backend);
    return TEST_EXIT();
}
