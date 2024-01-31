#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "inttypes.h"
#include "stdbool.h"

#include "hardware/pio.h"

#define PIO_VIDEO pio0
#define PIO_VIDEO_ADDR pio0
#define VIDEO_DMA_IRQ (DMA_IRQ_0)

#define TEXTMODE_COLS 53
#define TEXTMODE_ROWS 30

#define RGB888(r, g, b) ((r<<16) | (g << 8 ) | b )

#ifndef HDMI_BASE_PIN
#define HDMI_BASE_PIN (6)
#endif

#define HDMI_PIN_invert_diffpairs (1)
#define HDMI_PIN_RGB_notBGR (1)
#define beginHDMI_PIN_data (HDMI_BASE_PIN+2)
#define beginHDMI_PIN_clk (HDMI_BASE_PIN)


// TODO: Сделать настраиваемо
const uint8_t __scratch_y("hdmi_textmode_palette") textmode_palette[16] = {
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215
};



void v_drv_set_bg_color(uint8_t color); //используем номер цвета палитры
void v_drv_set_txt_palette(uint8_t i_color, uint8_t pal_inx);

inline void graphics_set_flashmode(bool flash_line, bool flash_frame) {
    // dummy
}


#ifdef __cplusplus
}
#endif
