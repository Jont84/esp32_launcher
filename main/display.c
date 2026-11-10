/*
 * display.c - puts the game's 320x240 indexed frame on the ILI9341.
 *
 * Panel set-up is the Keen port's verified keen_display.c. What differs:
 *   - 256-colour palette and the full 320x240 frame (no letterbox), the two
 *     changes listed in the Keen handoff;
 *   - synchronous: display_show() returns when the panel has the frame. A
 *     text game redraws only when something changes, so a display task isn't
 *     needed;
 *   - only the 20-line bands that changed since the last frame are sent. Most
 *     button presses change a few bands; an idle screen sends nothing.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "board.h"
#include "display.h"

static const char *TAG = "display";

#define BAND_H   20
#define BAND_PIX (BD_LCD_W * BAND_H)
#define N_BANDS  (BD_LCD_H / BAND_H)

static esp_lcd_panel_handle_t panel;
static uint16_t *band_buf[2];             /* DMA buffers, internal RAM */
static SemaphoreHandle_t band_free;       /* counts band buffers the DMA has released */
static uint8_t *last;                     /* the frame now on the panel, in PSRAM */
static uint16_t last_lut[256];
static bool have_last, backlight_on;

static bool IRAM_ATTR on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(band_free, &woken);
    return woken == pdTRUE;
}

static void lcd_init(void)
{
    gpio_config_t bl = { .pin_bit_mask = 1ULL << BD_PIN_LCD_BL, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl);
    gpio_set_level(BD_PIN_LCD_BL, 0);

    spi_bus_config_t bus = {
        .sclk_io_num = BD_PIN_LCD_SCLK,
        .mosi_io_num = BD_PIN_LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BAND_PIX * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(BD_LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = BD_PIN_LCD_DC,
        .cs_gpio_num = BD_PIN_LCD_CS,
        .pclk_hz = BD_LCD_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BD_LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t pcfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = BD_LCD_RGB_ORDER,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &pcfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, BD_LCD_INVERT));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, BD_LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, BD_LCD_MIRROR_X, BD_LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
}

void display_init(void)
{
    band_free = xSemaphoreCreateCounting(2, 2);
    for (int i = 0; i < 2; i++) {
        band_buf[i] = heap_caps_malloc(BAND_PIX * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        assert(band_buf[i]);
    }
    last = heap_caps_malloc(BD_LCD_W * BD_LCD_H, MALLOC_CAP_SPIRAM);
    if (!last) last = heap_caps_malloc(BD_LCD_W * BD_LCD_H, MALLOC_CAP_8BIT);
    assert(last);
    lcd_init();
    ESP_LOGI(TAG, "display ready: %dx%d at %d MHz", BD_LCD_W, BD_LCD_H, BD_LCD_CLOCK_HZ / 1000000);
}

int display_show(const uint8_t *fb, const uint16_t lut[256])
{
    static int next = 0;
    bool lut_changed = !have_last || memcmp(lut, last_lut, sizeof last_lut) != 0;
    int sent = 0;

    for (int b = 0; b < N_BANDS; b++) {
        const uint8_t *src = fb + b * BAND_PIX;
        if (!lut_changed && !memcmp(src, last + b * BAND_PIX, BAND_PIX)) continue;

        xSemaphoreTake(band_free, portMAX_DELAY);
        uint16_t *dst = band_buf[next];
        for (int i = 0; i < BAND_PIX; i += 4) {
            dst[i]     = lut[src[i]];
            dst[i + 1] = lut[src[i + 1]];
            dst[i + 2] = lut[src[i + 2]];
            dst[i + 3] = lut[src[i + 3]];
        }
        esp_lcd_panel_draw_bitmap(panel, 0, b * BAND_H, BD_LCD_W, (b + 1) * BAND_H, dst);
        next ^= 1;
        memcpy(last + b * BAND_PIX, src, BAND_PIX);
        sent++;
    }

    /* wait until both band buffers are back: the frame is on the panel */
    if (sent) {
        xSemaphoreTake(band_free, portMAX_DELAY);
        xSemaphoreTake(band_free, portMAX_DELAY);
        xSemaphoreGive(band_free);
        xSemaphoreGive(band_free);
    }
    memcpy(last_lut, lut, sizeof last_lut);
    have_last = true;

    if (!backlight_on) {        /* only after the first real frame: no panel garbage at power-up */
        gpio_set_level(BD_PIN_LCD_BL, 1);
        backlight_on = true;
    }
    return sent;
}
