/**
 * @file main.c
 * @brief Пример 13: Основной 3-канальный DC-DC преобразователь для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 13 (Лист 8 Power.SchDoc):
 *  1. Основной 3-канальный синхронный понижающий DC-DC конвертер D3: TPS65261RHBR (+V1, +V2);
 *  2. Цифровые потенциометры регулировки напряжения:
 *     - DA2 MCP4532 (+V1, адрес I2C 0x2C);
 *     - DA3 MCP4532 (+V2, адрес I2C 0x2D);
 *  3. Измерители мощности и тока INA226:
 *     - DD5 (+V1, адрес I2C 0x40, шунт R51 = 20 мОм);
 *     - DD6 (+V2, адрес I2C 0x41, шунт R62 = 20 мОм);
 *  4. Мониторинг сигнала Power Good (PG) от D3 через TCA9535 (P0.1, TP60).
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

static const char *TAG = "EX13_TPS65261_DCDC";

static led_strip_handle_t s_led_strip = NULL;

/* --------------------------------------------------------------------------
 * 1. Инициализация адресного светодиода LED1 (WS2812C, GPIO23, TP20)
 * ------------------------------------------------------------------------ */

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

/* --------------------------------------------------------------------------
 * 2. Задача мониторинга DC-DC конвертера TPS65261 и телеметрии INA226
 * ------------------------------------------------------------------------ */

static void dcdc_telemetry_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Запуск задачи мониторинга TPS65261 (+V1, +V2) и телеметрии INA226 (0x40, 0x41)");

    while (1) {
        /* 1. Чтение сигнала Power Good (PG) от D3 через TCA9535 (P0.1, TP60) */
        bool pg = false;
        if (levisw_bsp_power_good(&pg) != ESP_OK) {
            pg = false;
        }

        /* 2. Чтение телеметрии шины +V1 (DD5 INA226, 0x40) */
        float v1_v = 0.0f, v1_i_ma = 0.0f, v1_p_mw = 0.0f;
        esp_err_t err_v1 = levisw_bsp_ina226_read(LEVISW_INA_V1, &v1_v, NULL, &v1_i_ma, &v1_p_mw);

        /* 3. Чтение телеметрии шины +V2 (DD6 INA226, 0x41) */
        float v2_v = 0.0f, v2_i_ma = 0.0f, v2_p_mw = 0.0f;
        esp_err_t err_v2 = levisw_bsp_ina226_read(LEVISW_INA_V2, &v2_v, NULL, &v2_i_ma, &v2_p_mw);

        if (pg) {
            ESP_LOGI(TAG, "[TPS65261 DCDC] Power Good = 1 (OK)");
            if (err_v1 == ESP_OK) {
                ESP_LOGI(TAG, "  -> Шина +V1 (+V1EN=1): U = %.2f В, I = %.1f мА, P = %.1f мВт", v1_v, v1_i_ma, v1_p_mw);
            }
            if (err_v2 == ESP_OK) {
                ESP_LOGI(TAG, "  -> Шина +V2 (+V2EN=1): U = %.2f В, I = %.1f мА, P = %.1f мВт", v2_v, v2_i_ma, v2_p_mw);
            }
            app_led_set_rgb(0, 50, 0); /* Зеленый: питание в норме */
        } else {
            ESP_LOGE(TAG, "[TPS65261 DCDC] АВАРИЯ: Power Good = 0 (Сбой питания)!");
            app_led_set_rgb(60, 0, 0); /* Красный: авария */
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 13: DC-DC TPS65261 и INA226");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 13: Power.SchDoc Лист 8)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, MCP4532 уставки, INA226 калибровка) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые преобразователи +V1 и +V2 (V1EN=1, V2EN=1) */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 3. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());

    /* 4. Создание задачи телеметрии питания */
    xTaskCreate(dcdc_telemetry_task, "dcdc_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 13 успешно запущен. Мониторинг TPS65261 и INA226 активен.");
}
