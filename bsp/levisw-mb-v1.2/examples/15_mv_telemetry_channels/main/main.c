/**
 * @file main.c
 * @brief Пример 15: Каналы цифрового управления и телеметрии MV1, MV1e, MV2 для платы LeviSW-MB v1.2 (BSP copy).
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "levisw_mb_v1_2_pins.h"
#include "levisw_mb_v1_2_bsp.h"

static const char *TAG = "EX15_MV_TELEMETRY";

static led_strip_handle_t s_led_strip = NULL;

static esp_err_t app_led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LEVISW_PIN_LED_WS2812,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = { .invert_out = false }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags = { .with_dma = false }
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err == ESP_OK) {
        led_strip_clear(s_led_strip);
        ESP_LOGI(TAG, "Светодиод LED1 (WS2812C, GPIO23, TP20) инициализирован");
    }
    return err;
}

static void app_led_set_rgb(uint32_t r, uint32_t g, uint32_t b)
{
    if (s_led_strip != NULL) {
        led_strip_set_pixel(s_led_strip, 0, r, g, b);
        led_strip_refresh(s_led_strip);
    }
}

static void mv_telemetry_test_task(void *pvParameters)
{
    uint8_t wiper_v1 = 32;
    uint8_t wiper_v2 = 48;
    uint8_t wiper_uload = 64;

    ESP_LOGI(TAG, "Запуск тестового цикла каналов MV1, MV1e, MV2 (MCP4532 + INA226)");

    while (1) {
        wiper_v1 = (wiper_v1 == 32) ? 64 : 32;
        wiper_v2 = (wiper_v2 == 48) ? 80 : 48;
        wiper_uload = (wiper_uload == 64) ? 96 : 64;

        ESP_LOGI(TAG, "--- [MV1 / MV2 / MV1e TELEMETRY CYCLE] ---");

        if (levisw_bsp_pot_set_wiper(LEVISW_BUS_V1, wiper_v1) == ESP_OK) {
            ESP_LOGI(TAG, "[DA2 MCP4532] Уставка MV1 (+V1, 0x2C) установлена в wiper = %u", wiper_v1);
        }
        if (levisw_bsp_pot_set_wiper(LEVISW_BUS_V2, wiper_v2) == ESP_OK) {
            ESP_LOGI(TAG, "[DA3 MCP4532] Уставка MV2 (+V2, 0x2D) установлена в wiper = %u", wiper_v2);
        }
        if (levisw_bsp_pot_set_wiper(LEVISW_BUS_ULOAD, wiper_uload) == ESP_OK) {
            ESP_LOGI(TAG, "[DA4 MCP4532] Уставка MV1e (Uload, 0x2F) установлена в wiper = %u", wiper_uload);
        }

        float v1_v = 0.0f, v1_shunt_mv = 0.0f, v1_cur_ma = 0.0f, v1_pow_mw = 0.0f;
        if (levisw_bsp_ina226_read(LEVISW_INA_V1, &v1_v, &v1_shunt_mv, &v1_cur_ma, &v1_pow_mw) == ESP_OK) {
            ESP_LOGI(TAG, "[DD5 INA226 +V1 (0x40)] U: %.2f В | Shunt: %.2f мВ | I: %.1f мА | P: %.1f мВт",
                     v1_v, v1_shunt_mv, v1_cur_ma, v1_pow_mw);
        }

        float v2_v = 0.0f, v2_shunt_mv = 0.0f, v2_cur_ma = 0.0f, v2_pow_mw = 0.0f;
        if (levisw_bsp_ina226_read(LEVISW_INA_V2, &v2_v, &v2_shunt_mv, &v2_cur_ma, &v2_pow_mw) == ESP_OK) {
            ESP_LOGI(TAG, "[DD6 INA226 +V2 (0x41)] U: %.2f В | Shunt: %.2f мВ | I: %.1f мА | P: %.1f мВт",
                     v2_v, v2_shunt_mv, v2_cur_ma, v2_pow_mw);
        }

        app_led_set_rgb(15, 50, 10);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 15: Управление и телеметрия MV1, MV1e, MV2");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 15: ManageChannel Листы 8, 9, 10)");
    ESP_LOGI(TAG, "===========================================================");

    ESP_ERROR_CHECK(levisw_bsp_init());
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, true, true));
    ESP_ERROR_CHECK(app_led_init());

    xTaskCreate(mv_telemetry_test_task, "mv_telemetry_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 15 успешно запущен. Мониторинг и управление MV1/MV1e/MV2 активны.");
}
