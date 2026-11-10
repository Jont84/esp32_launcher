/*
 * The handheld's launcher: a menu of the games on the board.
 *
 * Lives in the factory slot; its ESP-IDF project is named "launcher", which
 * is how the games recognise it (launcher contract, Adventure D51):
 *   - each game sets the boot choice back to factory at start, only if the
 *     factory app is named "launcher", so any reset returns here;
 *   - "quit" in a game = save, then esp_restart();
 *   - picking a game here = esp_ota_set_boot_partition(slot); esp_restart();
 *   - each project keeps its settings in its own NVS namespace ("launcher"
 *     here: the last game played); nothing erases NVS.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "buttons.h"
#include "display.h"
#include "input.h"
#include "menu.h"

static const char *TAG = "launcher";

static const menu_entry games[] = {
    { "keen_app",      "Commander Keen", "Secret of the Oracle", MC_GREEN, false, "" },
    { "penguins_app",  "Penguins",       "6 worlds, 32 levels",  MC_BLUE,  false, "" },
    { "adventure_app", "Adventure",      "Robinson Crusoe",      MC_GOLD,  false, "" },
};
#define N_GAMES (int)(sizeof games / sizeof games[0])

static const esp_partition_t *slot(const char *label)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
}

/* Which slots hold a valid app, and their version strings. */
static void scan(menu_state *m)
{
    memset(m, 0, sizeof *m);
    m->n = N_GAMES;
    memcpy(m->e, games, sizeof games);
    for (int i = 0; i < m->n; i++) {
        const esp_partition_t *p = slot(m->e[i].slot);
        esp_app_desc_t desc;
        if (p && esp_ota_get_partition_description(p, &desc) == ESP_OK) {
            m->e[i].installed = true;
            snprintf(m->e[i].version, sizeof m->e[i].version, "%.12s", desc.version);
            ESP_LOGI(TAG, "%s: %s %s", m->e[i].slot, desc.project_name, desc.version);
        } else {
            ESP_LOGI(TAG, "%s: %s", m->e[i].slot, p ? "empty" : "no such partition");
        }
    }
}

static int load_last(void)
{
    nvs_handle_t h;
    uint8_t v = 0;
    if (nvs_open("launcher", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "last", &v);
        nvs_close(h);
    }
    return v < N_GAMES ? v : 0;
}

static void save_last(int i)
{
    nvs_handle_t h;
    if (nvs_open("launcher", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "last", (uint8_t)i);
    nvs_commit(h);
    nvs_close(h);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) ESP_LOGE(TAG, "NVS: %s (not erased: it holds every game's saves)", esp_err_to_name(err));

    display_init();
    input_init();

    static menu_state m;
    scan(&m);
    m.sel = load_last();

    uint8_t *fb = heap_caps_malloc(MENU_W * MENU_H, MALLOC_CAP_SPIRAM);
    if (!fb) fb = heap_caps_malloc(MENU_W * MENU_H, MALLOC_CAP_8BIT);
    assert(fb);
    static uint16_t lut[256];
    for (int i = 0; i < 256; i++) lut[i] = display_rgb565(menu_palette[i][0], menu_palette[i][1], menu_palette[i][2]);

    /* A button still held from the game that just quit must not count as a
     * press here: act only on buttons pressed after everything was released. */
    uint32_t prev = 0xFFFFFFFF;
    TickType_t wake = xTaskGetTickCount();
    while (1) {
        uint32_t now = input_get();
        uint32_t down = now & ~prev;
        prev = now;

        int pick = menu_press(&m, down);
        if (pick >= 0) {
            const esp_partition_t *p = slot(m.e[pick].slot);
            snprintf(m.status, sizeof m.status, "Starting %s...", m.e[pick].title);
            menu_draw(&m, fb);
            display_show(fb, lut);
            save_last(pick);
            if (p && esp_ota_set_boot_partition(p) == ESP_OK) {
                ESP_LOGI(TAG, "starting %s", m.e[pick].slot);
                vTaskDelay(pdMS_TO_TICKS(150));
                esp_restart();
            }
            snprintf(m.status, sizeof m.status, "%s won't start: its image is damaged.", m.e[pick].title);
        }
        menu_draw(&m, fb);
        display_show(fb, lut);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(33));
    }
}
