#include "graphics.h"
#include <stdio.h>
#include <string.h>
#include <hardware/gpio.h>

#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "fnt6x8.h"

#define GPPWM   29       // GPIO2 is PWM output
#define TV_BASE_PIN    14        // TV_BASE_PIN connected to RCA+ pin via 330 ohm
#define TV_SECOND_PIN   (TV_BASE_PIN+1)      // TV_SECOND_PIN connected to RCA+ pin via 1k ohm
#define BASE_MASK     (1 << TV_BASE_PIN) // bit mask for TV_BASE_PIN
#define SECOND_MASK     (1 << TV_SECOND_PIN) // bit mask for TV_SECOND_PIN
#define SYNC    gpio_put_masked(BASE_MASK | SECOND_MASK, 0)             // TV_BASE_PIN='L' and TV_SECOND_PIN='L'
#define WHITE   gpio_put_masked(BASE_MASK | SECOND_MASK, BASE_MASK | SECOND_MASK)     // TV_BASE_PIN='H' and TV_SECOND_PIN='H'
#define BLACK   gpio_put_masked(BASE_MASK | SECOND_MASK, SECOND_MASK)           // TV_BASE_PIN='L' and TV_SECOND_PIN='H'
#define GRAY    gpio_put_masked(BASE_MASK | SECOND_MASK, BASE_MASK)           // TV_BASE_PIN='H' and TV_SECOND_PIN='L'
#define V_BASE  24      // horizontal line number to start displaying VRAM

#define BDOT    0       // black dot
#define WDOT    1       // white dot
#define GDOT    2       // gray dot
#define ZDOT    3       // dummy dot

static int32_t scanline; // linecount?

//активный видеорежим
static enum graphics_mode_t graphics_mode = GRAPHICSMODE_DEFAULT;

//буфер  палитры 256 цветов в формате R8G8B8
static uint32_t palette[256];


//графический буфер
static uint8_t* __scratch_y("hdmi_ptr_1") graphics_buffer = NULL;
static int graphics_buffer_width = 0;
static int graphics_buffer_height = 0;
static int graphics_buffer_shift_x = 0;
static int graphics_buffer_shift_y = 0;

//текстовый буфер
uint8_t* text_buffer = NULL;

//выбор видеорежима
void graphics_set_mode(enum graphics_mode_t mode) {
    graphics_mode = mode;
    clrScr(0);
};

void graphics_set_palette(uint8_t i, uint32_t color888) {
    palette[i] = color888 & 0x00ffffff;
};

void graphics_set_buffer(uint8_t* buffer, uint16_t width, uint16_t height) {
    graphics_buffer = buffer;
    graphics_buffer_width = width;
    graphics_buffer_height = height;
};


// initialize video and LED GPIO
void init_video_and_led_GPIO( ) {
    // initialize TV_BASE_PIN and TV_SECOND_PIN for masked output
    gpio_init(TV_BASE_PIN);
    gpio_init(TV_SECOND_PIN);

    gpio_set_dir(TV_BASE_PIN, GPIO_OUT);
    gpio_set_dir(TV_SECOND_PIN, GPIO_OUT);

    gpio_init_mask(BASE_MASK | SECOND_MASK);

}

// to generate horizontal sync siganl
void hsync( void ) {
    SYNC;
    sleep_us(5);
    BLACK;
    sleep_us(7);
}

// to generate vertical sync siganl
void vsync ( void ) {
    SYNC;
    sleep_us(10);
    sleep_us(5);
    sleep_us(10);
    BLACK;
    sleep_us(5);

    SYNC;
    sleep_us(10);
    sleep_us(5);
    sleep_us(10);
    BLACK;
    sleep_us(5);
}

// handler for holizontal line processing
void horizontal_line( ) {
    // Clear the interrupt flag that brought us here
    pwm_clear_irq(pwm_gpio_to_slice_num(GPPWM));

    // vertical synchronization duration
    if (scanline >= 3 && scanline <= 5) {
        vsync();    // vertical SYNC

    // VRAM drawing area
    } else if (scanline >= V_BASE && scanline < V_BASE + SCREEN_HEIGHT) {
        hsync();
        // left blank??
        BLACK;
        sleep_us(0);    // should be tuned

        int y = scanline - V_BASE;
        switch (graphics_mode) {
            case GRAPHICSMODE_DEFAULT:
                for (int x = 0; x < graphics_buffer_width; x++) {
                    int c = graphics_buffer[x+y*graphics_buffer_width];
                    switch (c) {
                        case BDOT:  // black
                            BLACK;
                        break;
                        case WDOT:  // white
                            WHITE;
                        break;
                        case GDOT:  // gray
                            GRAY;
                        break;
                        //case ZDOT:  // dummy
                        //break;
                        default:
                            BLACK;
                    }
                }
            break;
            case TEXTMODE_DEFAULT: {
                BLACK;
                for (int x = 0; x < TEXTMODE_COLS; x++) {
                    const uint16_t offset = (y / 8) * (TEXTMODE_COLS * 2) + x * 2;
                    const uint8_t c = text_buffer[offset];
                    const uint8_t colorIndex = text_buffer[offset + 1];
                    uint8_t glyph_row = fnt6x8[c * 8 + y % 8];

                    for (int bit = 6; bit--;) {
                        /*
                        *output_buffer++ = glyph_row & 1
                                               ? textmode_palette[colorIndex & 0xf] //цвет шрифта
                                               : textmode_palette[colorIndex >> 4]; //цвет фона
*/
                        glyph_row & 1 ? WHITE: BLACK;

                        glyph_row >>= 1;
                    }
                }
                BLACK;
            }
        }
        // right blank??
        BLACK;
        sleep_us(0);
    } else {
        hsync();
        BLACK;
    }
    // count up scan line
    scanline++;
    // if scan line reach to max
    if (scanline > 262) {
        scanline = 1;
    }
}

// initialize and start PWM interrupt by 64us period
void enable_PWM_interrupt(  ) {

    // GPPWM pin is the PWM output
    gpio_set_function(GPPWM, GPIO_FUNC_PWM);
    // Figure out which slice we just connected to the GPPWM pin
    uint slice_num = pwm_gpio_to_slice_num(GPPWM);

    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_priority(PWM_IRQ_WRAP, 0xC0);   // somehow this is needed if you compile with release option
    irq_set_exclusive_handler(PWM_IRQ_WRAP, horizontal_line);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Set counter wrap value to generate PWM interrupt by this value
    pwm_set_wrap(slice_num, 7999);
    // Load the configuration into our PWM slice, and set it running.
    pwm_set_enabled(slice_num, true);
}

void graphics_init() {
    // FIXME сделать конфигурацию пользователем
    graphics_set_palette(200, RGB888(0x00, 0x00, 0x00)); //black
    graphics_set_palette(201, RGB888(0x00, 0x00, 0xC4)); //blue
    graphics_set_palette(202, RGB888(0x00, 0xC4, 0x00)); //green
    graphics_set_palette(203, RGB888(0x00, 0xC4, 0xC4)); //cyan
    graphics_set_palette(204, RGB888(0xC4, 0x00, 0x00)); //red
    graphics_set_palette(205, RGB888(0xC4, 0x00, 0xC4)); //magenta
    graphics_set_palette(206, RGB888(0xC4, 0x7E, 0x00)); //brown
    graphics_set_palette(207, RGB888(0xC4, 0xC4, 0xC4)); //light gray
    graphics_set_palette(208, RGB888(0x4E, 0x4E, 0x4E)); //dark gray
    graphics_set_palette(209, RGB888(0x4E, 0x4E, 0xDC)); //light blue
    graphics_set_palette(210, RGB888(0x4E, 0xDC, 0x4E)); //light green
    graphics_set_palette(211, RGB888(0x4E, 0xF3, 0xF3)); //light cyan
    graphics_set_palette(212, RGB888(0xDC, 0x4E, 0x4E)); //light red
    graphics_set_palette(213, RGB888(0xF3, 0x4E, 0xF3)); //light magenta
    graphics_set_palette(214, RGB888(0xF3, 0xF3, 0x4E)); //yellow
    graphics_set_palette(215, RGB888(0xFF, 0xFF, 0xFF)); //white

    init_video_and_led_GPIO();
    enable_PWM_interrupt();
}

void graphics_set_bgcolor(uint32_t color888) //определяем зарезервированный цвет в палитре
{
    graphics_set_palette(255, color888);
};

void graphics_set_offset(int x, int y) {
    graphics_buffer_shift_x = x;
    graphics_buffer_shift_y = y;
};

void graphics_set_textbuffer(uint8_t* buffer) {
    text_buffer = buffer;
};


void clrScr(const uint8_t color) {
    uint16_t* t_buf = (uint16_t *)text_buffer;
    int size = TEXTMODE_COLS * TEXTMODE_ROWS;

    while (size--) *t_buf++ = color << 4 | ' ';
}
