/* SPDX-License-Identifier: MIT */
/* Host platform: window, framebuffer, input events and clock.
 * Backends: SDL3 (platform/src/platform_sdl3.c) and null (tests, CI).
 * Nothing above this layer includes SDL headers. */
#ifndef RC_PLATFORM_H
#define RC_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rc_event_type {
    RC_EV_NONE = 0,
    RC_EV_QUIT,
    RC_EV_MOUSE_MOVE,   /* x, y in framebuffer pixels */
    RC_EV_MOUSE_BUTTON, /* button 1 = left, 2 = middle, 3 = right; down */
    RC_EV_KEY,          /* scancode (USB HID usage id), down */
    RC_EV_FOCUS         /* down = 1 gained, 0 lost */
} rc_event_type;

typedef struct rc_event {
    rc_event_type type;
    int x, y;
    int button;
    int scancode;
    int down;
} rc_event;

typedef enum rc_pixel_format {
    RC_PIXFMT_RGB565 = 0,
    RC_PIXFMT_XRGB1555 = 1
} rc_pixel_format;

/* Name of the compiled backend: "sdl3" or "null". */
const char *rc_platform_backend(void);

/* Open the window (w x h is the initial framebuffer size). Returns 0 on success. */
int rc_platform_init(const char *title, int w, int h);
void rc_platform_shutdown(void);

/* Fetch one pending event. Returns 1 if *ev was filled, 0 if the queue is empty. */
int rc_platform_poll(rc_event *ev);

/* Show a 16-bit frame. The window keeps the aspect ratio (letterbox) and scales with
 * nearest-neighbour filtering. pitch is in bytes. */
void rc_platform_present16(const void *pixels, int w, int h, int pitch, rc_pixel_format fmt);

/* Milliseconds since an arbitrary start point, monotonic. */
uint64_t rc_platform_ticks_ms(void);

#ifdef __cplusplus
}
#endif
#endif
