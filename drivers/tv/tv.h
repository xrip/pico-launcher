#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

#define SCREEN_WIDTH (320)
#define SCREEN_HEIGHT (240)

#define TEXTMODE_COLS (53)
#define TEXTMODE_ROWS (30)

#define RGB888(r, g, b) ((r<<16) | (g << 8 ) | b )

// TODO: Сделать настраиваемо
static const uint8_t textmode_palette[16] = {
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215
};


static void graphics_set_flashmode(bool flash_line, bool flash_frame) {
    // dummy
}


#ifdef __cplusplus
}
#endif
