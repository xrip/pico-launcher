//
// Created by xrip on 22.01.2024.
//
#include "SDL2/SDL.h"

#include "stdbool.h"
#include "stdint.h"
#include "sdl.h"

SDL_TLSID cpu_core_ids;
SDL_Window* window;
SDL_Surface* screen;


unsigned int * pixels;
static uint8_t* text_buffer = NULL;
static uint8_t* graphics_buffer = NULL;

static unsigned int graphics_buffer_width = 0;
static unsigned int graphics_buffer_height = 0;
static int graphics_buffer_shift_x = 0;
static int graphics_buffer_shift_y = 0;

enum graphics_mode_t graphics_mode = VGA_320x200x256;

void graphics_init() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO);

    cpu_core_ids = SDL_TLSCreate();

    window = SDL_CreateWindow("pico",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              640 * 2, 480 * 2,
                              SDL_WINDOW_SHOWN);
    screen = SDL_GetWindowSurface(window);

    const SDL_Surface * drawsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, screen->format->BitsPerPixel,
                                            screen->format->Rmask, screen->format->Gmask, screen->format->Bmask,
                                            screen->format->Amask);
    pixels = drawsurface->pixels;
}

void inline graphics_set_mode(const enum graphics_mode_t mode) {
    graphics_mode = -1;
    clrScr(0);
    graphics_mode = mode;
}

void graphics_set_buffer(uint8_t* buffer, const uint16_t width, const uint16_t height) {
    graphics_buffer = buffer;
    graphics_buffer_width = width;
    graphics_buffer_height = height;
}

void graphics_set_textbuffer(uint8_t* buffer) {
    text_buffer = buffer;
}

void graphics_set_offset(const int x, const int y) {
    graphics_buffer_shift_x = x;
    graphics_buffer_shift_y = y;
}

// Грязный хак, какбудто бы у нас всегда второе ядро - видео
int core1_thread_func(void *entry) {
    SDL_TLSSet(cpu_core_ids, (void *) 2, 0);
    // todo locking and cleanup
    ((void (*)(void)) entry)();
}

void multicore_launch_core1(void (*entry)(void)) {
    SDL_CreateThread(core1_thread_func, "Core 1", (void *) entry);
}