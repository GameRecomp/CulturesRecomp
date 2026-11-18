/* SPDX-License-Identifier: MIT */
#include "rc/platform.h"

#include <SDL3/SDL.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int tex_w, tex_h;
static rc_pixel_format tex_fmt;

const char *rc_platform_backend(void) { return "sdl3"; }

int rc_platform_init(const char *title, int w, int h) {
    if (w <= 0 || h <= 0) return -1;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }
    window = SDL_CreateWindow(title, w, h, SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        window = NULL;
        SDL_Quit();
        return -1;
    }
    return 0;
}

void rc_platform_shutdown(void) {
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    texture = NULL;
    renderer = NULL;
    window = NULL;
    tex_w = tex_h = 0;
    SDL_Quit();
}

static int map_button(Uint8 b) {
    switch (b) {
    case SDL_BUTTON_LEFT: return 1;
    case SDL_BUTTON_MIDDLE: return 2;
    case SDL_BUTTON_RIGHT: return 3;
    default: return 0;
    }
}

int rc_platform_poll(rc_event *ev) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (renderer) SDL_ConvertEventToRenderCoordinates(renderer, &e);
        ev->type = RC_EV_NONE;
        ev->x = ev->y = ev->button = ev->scancode = ev->down = 0;
        switch (e.type) {
        case SDL_EVENT_QUIT:
            ev->type = RC_EV_QUIT;
            return 1;
        case SDL_EVENT_MOUSE_MOTION:
            ev->type = RC_EV_MOUSE_MOVE;
            ev->x = (int)e.motion.x;
            ev->y = (int)e.motion.y;
            return 1;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            ev->type = RC_EV_MOUSE_BUTTON;
            ev->button = map_button(e.button.button);
            ev->down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            ev->x = (int)e.button.x;
            ev->y = (int)e.button.y;
            return 1;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            ev->type = RC_EV_KEY;
            ev->scancode = (int)e.key.scancode; /* SDL scancodes are USB HID usage ids */
            ev->down = e.type == SDL_EVENT_KEY_DOWN;
            return 1;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            ev->type = RC_EV_FOCUS;
            ev->down = e.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
            return 1;
        default:
            break; /* not interesting; keep draining */
        }
    }
    ev->type = RC_EV_NONE;
    return 0;
}

void rc_platform_present16(const void *pixels, int w, int h, int pitch, rc_pixel_format fmt) {
    if (!renderer || w <= 0 || h <= 0) return;
    if (!texture || w != tex_w || h != tex_h || fmt != tex_fmt) {
        if (texture) SDL_DestroyTexture(texture);
        SDL_PixelFormat sdl_fmt =
            fmt == RC_PIXFMT_RGB565 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_XRGB1555;
        texture = SDL_CreateTexture(renderer, sdl_fmt, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!texture) {
            SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
            return;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        tex_w = w;
        tex_h = h;
        tex_fmt = fmt;
    }
    SDL_UpdateTexture(texture, NULL, pixels, pitch);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

uint64_t rc_platform_ticks_ms(void) { return (uint64_t)SDL_GetTicks(); }
