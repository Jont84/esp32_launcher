/*
 * input.c - polls the I2C joystick controller (the Keen port's verified
 * keen_input.c, without the BOOT button: it's out of reach in the enclosure).
 *
 * A task on core 0 reads the controller 100 times a second and keeps the
 * latest state in one word. If the controller is unplugged it keeps retrying.
 * Bits are BTN_* (buttons.h), the same layout as Keen's KEEN_BTN_*.
 */
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "board.h"
#include "input.h"
#include "buttons.h"

static const char *TAG = "input";

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t ctrl;
static _Atomic uint32_t state;
static _Atomic bool connected_now;
static bool found_at_start;

static int read_reg(uint8_t reg)
{
    uint8_t v;
    if (i2c_master_transmit(ctrl, &reg, 1, 20) != ESP_OK) return -1;
    if (i2c_master_receive(ctrl, &v, 1, 20) != ESP_OK) return -1;
    return v;
}

/* The button registers report events; these values mean "held now" (verified on Keen) */
static bool held(int v) { return v == 0 || v == 2 || v == 5 || v == 6; }

static bool controller_connect(void)
{
    if (i2c_master_probe(bus, BD_CTRL_ADDR, 50) != ESP_OK) return false;
    if (!ctrl) {
        i2c_device_config_t dc = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = BD_CTRL_ADDR,
            .scl_speed_hz = 400000,
        };
        if (i2c_master_bus_add_device(bus, &dc, &ctrl) != ESP_OK) return false;
    }
    return true;
}

static void input_task(void *arg)
{
    bool connected = found_at_start;
    int fails = 0;
    TickType_t last_try = 0;

    while (1) {
        uint32_t s = 0;

        if (!connected && xTaskGetTickCount() - last_try > pdMS_TO_TICKS(1000)) {
            last_try = xTaskGetTickCount();
            connected = controller_connect();
            if (connected) ESP_LOGI(TAG, "controller found at 0x%02X", BD_CTRL_ADDR);
        }

        if (connected) {
            static const struct { uint8_t reg; uint32_t bit; } btn[] = {
                { BD_CTRL_REG_A, BTN_A }, { BD_CTRL_REG_B, BTN_B },
                { BD_CTRL_REG_C, BTN_C }, { BD_CTRL_REG_D, BTN_D },
                { BD_CTRL_REG_OK, BTN_OK },
            };
            int x = read_reg(BD_CTRL_REG_X);
            int y = read_reg(BD_CTRL_REG_Y);
            bool ok = x >= 0 && y >= 0;
            if (ok) {
                if (BD_CTRL_INVERT_X) x = 255 - x;
                if (BD_CTRL_INVERT_Y) y = 255 - y;
                if (x < 128 - BD_CTRL_DEADZONE) s |= BTN_LEFT;
                if (x > 128 + BD_CTRL_DEADZONE) s |= BTN_RIGHT;
                if (y < 128 - BD_CTRL_DEADZONE) s |= BTN_UP;
                if (y > 128 + BD_CTRL_DEADZONE) s |= BTN_DOWN;
            }
            for (int i = 0; ok && i < 5; i++) {
                int v = read_reg(btn[i].reg);
                if (v < 0) ok = false;
                else if (held(v)) s |= btn[i].bit;
            }
            if (!ok) {
                /* keep the previous state through a glitch; give up after ~0.5 s */
                if (++fails > 50) {
                    ESP_LOGW(TAG, "controller not responding - will keep retrying");
                    connected = false;
                    fails = 0;
                    s = 0;
                } else {
                    s = atomic_load(&state);
                }
            } else {
                fails = 0;
            }
        }
        atomic_store(&connected_now, connected);
        atomic_store(&state, s);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void input_init(void)
{
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BD_PIN_I2C_SDA,
        .scl_io_num = BD_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc, &bus));
    found_at_start = controller_connect();
    atomic_store(&connected_now, found_at_start);
    if (!found_at_start) ESP_LOGW(TAG, "no controller at 0x%02X yet - will keep looking", BD_CTRL_ADDR);
    xTaskCreatePinnedToCore(input_task, "input", 4096, NULL, 4, NULL, 0);
}

uint32_t input_get(void) { return atomic_load(&state); }
bool input_connected(void) { return atomic_load(&connected_now); }
void *input_i2c_bus(void) { return bus; }
