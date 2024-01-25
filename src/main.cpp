#include <cstdlib>
#include <cstring>
#include <hardware/clocks.h>
#include <hardware/flash.h>
#include <hardware/structs/vreg_and_chip_reset.h>
#include <pico/bootrom.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>

extern "C" {
#ifdef TFT
#include "st7789.h"
#endif
#ifdef HDMI
#include "hdmi.h"
#endif
#ifdef VGA
#include "vga.h"
bool cursor_blink_state = false;
uint8_t CURSOR_X, CURSOR_Y = 0;
uint8_t manager_started = false;
#endif

#include "ps2.h"
}


#include "ff.h"
#include "nespad.h"

static FATFS fs;
semaphore vga_start_semaphore;
#define DISP_WIDTH (320)
#define DISP_HEIGHT (240)

struct UF2_Block_t {
    // 32 byte header
    uint32_t magicStart0;
    uint32_t magicStart1;
    uint32_t flags;
    uint32_t targetAddr;
    uint32_t payloadSize;
    uint32_t blockNo;
    uint32_t numBlocks;
    uint32_t fileSize; // or familyID;
    uint8_t data[476];
    uint32_t magicEnd;
} UF2_Block_t;


uint8_t SCREEN[DISP_HEIGHT][DISP_WIDTH];

void __time_critical_func(render_core)() {
    multicore_lockout_victim_init();
    graphics_init();

    auto* buffer = &SCREEN[0][0];
    graphics_set_buffer(buffer, DISP_WIDTH, DISP_HEIGHT);
    graphics_set_textbuffer(buffer);
    graphics_set_bgcolor(0x000000);
    graphics_set_offset(0, 0);
    graphics_set_mode(TEXTMODE_80x30);
    clrScr(1);

    sem_acquire_blocking(&vga_start_semaphore);
#if 1
    // 60 FPS loop
#define frame_tick (16666)
    uint64_t tick = time_us_64();
    uint64_t last_renderer_tick = tick;
    uint64_t last_input_tick = tick;
    while (true) {
#ifdef TFT
        if (tick >= last_renderer_tick + frame_tick) {
            refresh_lcd();
            last_renderer_tick = tick;
        }
#endif
        // Every 5th frame
        if (tick >= last_input_tick + frame_tick * 5) {
            nespad_read();
            last_input_tick = tick;
        }
        tick = time_us_64();

        tight_loop_contents();
    }

    __unreachable();
#endif
}

void __always_inline reboot_to_application() {
    multicore_reset_core1();

    asm volatile (
        "mov r0, %[start]\n"
        "ldr r1, =%[vtable]\n"
        "str r0, [r1]\n"
        "ldmia r0, {r0, r1}\n"
        "msr msp, r0\n"
        "bx r1\n"
        :: [start] "r" (XIP_BASE + 0x100), [vtable] "X" (PPB_BASE + M0PLUS_VTOR_OFFSET)
    );

    __builtin_unreachable();
}

void __not_in_flash_func(load_firmware)(const char pathname[256]) {
    UINT bytes_read = 0;
    uint8_t buffer[FLASH_SECTOR_SIZE];
    struct UF2_Block_t uf2_block{};
    FIL file;

    constexpr int window_y = (TEXTMODE_ROWS - 5) / 2;
    constexpr int window_x = (TEXTMODE_COLS - 43) / 2;

    draw_window("Loading firmware", window_x, window_y, 43, 5);
    draw_text("Loading...", window_x + 1, window_y + 2, 10, 1);
    sleep_ms(100);

    if (FR_OK == f_open(&file, pathname, FA_READ)) {
        uint32_t flash_target_offset = 0;
        uint32_t data_sector_index = 0;
        FILINFO fileinfo;
        f_stat(pathname, &fileinfo);

        multicore_lockout_start_blocking();
        const uint32_t ints = save_and_disable_interrupts();

        do {
            f_read(&file, &uf2_block, sizeof uf2_block, &bytes_read);
            memcpy(buffer + data_sector_index, uf2_block.data, 256);
            data_sector_index += 256;

            if ((data_sector_index == FLASH_SECTOR_SIZE) || (bytes_read == 0)) {
                data_sector_index = 0;

                //подмена загрузчика boot2 прошивки на записанный ранее
                if (flash_target_offset == 0) {
                    memcpy(buffer, (uint8_t *) XIP_BASE, 256);
                }

                flash_range_erase(flash_target_offset, FLASH_SECTOR_SIZE);
                flash_range_program(flash_target_offset, buffer, FLASH_SECTOR_SIZE);

                gpio_put(PICO_DEFAULT_LED_PIN, (flash_target_offset >> 13) & 1);

                flash_target_offset += FLASH_SECTOR_SIZE;
            }
        } while (bytes_read != 0);

        restore_interrupts(ints);
        multicore_lockout_end_blocking();

        gpio_put(PICO_DEFAULT_LED_PIN, true);
    }
    f_close(&file);
}


typedef struct __attribute__((__packed__)) {
    bool is_directory;
    bool is_executable;
    size_t size;
    char filename[79];
    char short_filename[12 + 1];
} file_item_t;

constexpr int max_files = 1000;
static file_item_t fileItems[max_files];

void __not_in_flash_func(filebrowser)(const char pathname[256], const char* executables) {
    bool debounce = true;
    char basepath[256];
    char tmp[TEXTMODE_COLS + 1];
    strcpy(basepath, pathname);
    constexpr int per_page = TEXTMODE_ROWS - 3;

    DIR dir;
    FILINFO fileInfo;

    if (FR_OK !=  f_mount(&fs, "", 1)) {
        draw_text("SD Card not inserted or SD Card error!", 0, 0, 12, 0);
        while (true) { sleep_ms(100); /*TODO: reboot? */ }
    }

    while (true) {
        memset(fileItems, 0, sizeof(file_item_t) * max_files);
        int total_files = 0;

        snprintf(tmp, TEXTMODE_COLS, "SDCARD:\\%s", basepath);
        draw_window(tmp, 0, 0, TEXTMODE_COLS, TEXTMODE_ROWS - 1);
        memset(tmp, ' ', TEXTMODE_COLS);

#ifndef TFT
        draw_text(tmp, 0, 29, 0, 0);
        auto off = 0;
        draw_text((char *) "START", off, 29, 7, 0);
        off += 5;
        draw_text((char *) " Run at cursor ", off, 29, 0, 3);
        off += 16;
        draw_text((char *) "SELECT", off, 29, 7, 0);
        off += 6;
        draw_text((char *) " Run previous  ", off, 29, 0, 3);
        off += 16;
        draw_text((char *) "ARROWS", off, 29, 7, 0);
        off += 6;
        draw_text((char *) " Navigation    ", off, 29, 0, 3);
        off += 16;
        draw_text((char *) "A/Z", off, 29, 7, 0);
        off += 3;
        draw_text((char *) " USB DRV ", off, 29, 0, 3);
#endif

        if (FR_OK != f_opendir(&dir, basepath)) {
            draw_text((char *) "Failed to open directory", 1, 1, 4, 0);
            while (true) { sleep_ms(100); }
        }

        if (strlen(basepath) > 0) {
            strcpy(fileItems[total_files].filename, "..\0");
            fileItems[total_files].is_directory = true;
            fileItems[total_files].size = 0;
            total_files++;
        }

        while (f_readdir(&dir, &fileInfo) == FR_OK &&
               fileInfo.fname[0] != '\0' &&
               total_files < max_files
        ) {
            // Set the file item properties
            fileItems[total_files].is_directory = fileInfo.fattrib & AM_DIR;
            fileItems[total_files].size = fileInfo.fsize;
            // Extract the extension from the file name
            const char* extension = strrchr(fileInfo.fname, '.');
            if (extension != nullptr && strncmp(executables, extension + 1, 3) == 0) {
                fileItems[total_files].is_executable = true;
            }
            strncpy(fileItems[total_files].filename, fileInfo.fname, 78);
            total_files++;
        }
        f_closedir(&dir);


        if (total_files > max_files) {
            draw_text((char *) " Too many files!! ", TEXTMODE_COLS - 17, 0, 12, 3);
        }

        int offset = 0;
        int current_item = 0;

        while (true) {
            sleep_ms(100);

            if (!debounce) {
                debounce = (nespad_state & DPAD_START) == 0;
            }

            if ((nespad_state & DPAD_START) != 0) {
                gpio_put(PICO_DEFAULT_LED_PIN, true);
                // watchdog_enable(100, true);
            }

            if (nespad_state & DPAD_A) {
                clrScr(1);
                draw_text((char *) "Mount me as USB drive...", 30, 15, 7, 1);
                //in_flash_drive();
            }

            if ((nespad_state & DPAD_DOWN) != 0) {
                if ((offset + (current_item + 1) < total_files)) {
                    if ((current_item + 1) < per_page) {
                        current_item++;
                    } else {
                        offset++;
                    }
                }
            }

            if ((nespad_state & DPAD_UP) != 0) {
                if (current_item > 0) {
                    current_item--;
                } else if (offset > 0) {
                    offset--;
                }
            }

            if ((nespad_state & DPAD_RIGHT) != 0) {
                offset += per_page;
                if (offset + (current_item + 1) > total_files) {
                    offset = total_files - (current_item + 1);
                }
            }

            if ((nespad_state & DPAD_LEFT) != 0) {
                if (offset > per_page) {
                    offset -= per_page;
                } else {
                    offset = 0;
                    current_item = 0;
                }
            }

            if (debounce && (nespad_state & DPAD_START) != 0) {
                auto file_at_cursor = fileItems[offset + current_item];

                if (file_at_cursor.is_directory) {
                    if (strcmp(file_at_cursor.filename, "..") == 0) {
                        const char* lastBackslash = strrchr(basepath, '\\');
                        if (lastBackslash != nullptr) {
                            size_t length = lastBackslash - basepath;
                            basepath[length] = '\0';
                        }
                    } else {
                        sprintf(basepath, "%s\\%s", basepath, file_at_cursor.filename);
                    }
                    debounce = false;
                    break;
                }

                if (file_at_cursor.is_executable) {
                    sprintf(tmp, "%s\\%s", basepath, file_at_cursor.filename);
                    return load_firmware(tmp);
                }
            }

            for (int i = 0; i < per_page; i++) {
                const auto item = fileItems[offset + i];
                uint8_t color = 11;
                uint8_t bg_color = 1;

                if (i == current_item) {
                    color = 0;
                    bg_color = 3;
                    memset(tmp, 0xCD, TEXTMODE_COLS - 2);
                    tmp[TEXTMODE_COLS - 2] = '\0';
                    draw_text(tmp, 1, per_page + 1, 11, 1);
                    snprintf(tmp, TEXTMODE_COLS - 2, " Size: %iKb, File %lu of %i ", item.size / 1024, offset + i + 1,
                             total_files);
                    draw_text(tmp, 2, per_page + 1, 14, 3);
                }

                const auto len = strlen(item.filename);
                color = item.is_directory ? 15 : color;
                color = item.is_executable ? 10 : color;
                //color = strstr((char *)rom_filename, item.filename) != nullptr ? 13 : color;
                memset(tmp, ' ', TEXTMODE_COLS - 2);
                tmp[TEXTMODE_COLS - 2] = '\0';
                memcpy(&tmp, item.filename, len < TEXTMODE_COLS - 2 ? len : TEXTMODE_COLS - 2);
                draw_text(tmp, 1, i + 1, color, bg_color);
            }
        }
    }
}

int main() {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(10);
    set_sys_clock_khz(378 * 1000, true);

    keyboard_init();
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);

    for (int i = 2; i--;) {
        nespad_read();
        sleep_ms(50);

        // Boot to USB FIRMWARE UPDATE mode
        if ((nespad_state & DPAD_START) != 0) {
            reset_usb_boot(0, 0);
        }

        // Run launcher
        if ((nespad_state & DPAD_SELECT) != 0) {
            sem_init(&vga_start_semaphore, 0, 1);
            multicore_launch_core1(render_core);
            sem_release(&vga_start_semaphore);

            sleep_ms(250);
            filebrowser("", "uf2");
        }
    }


    reboot_to_application();

    __unreachable();
}
