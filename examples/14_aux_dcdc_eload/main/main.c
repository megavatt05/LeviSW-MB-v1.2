/**
 * @file main.c
 * @brief Пример 14: Вспомогательные DC-DC L1/V5 и коррекция порога L1ADJ для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 14 (Лист 9 Power.SchDoc):
 *  1. Вспомогательные импульсные стабилизаторы DP1, DP2: LM22675MRX-5.0 (формирование шин L1 / V5);
 *  2. Эквивалент нагрузки (eLOAD) и управление включением через сигнал CEN_L1 (eLOADEN, бит P0.0 на DD3 TCA9535, TP59);
 *  3. Цифровой потенциометр DA4: MCP4532T-104E/MS (адрес I2C 0x2F) для коррекции порога срабатывания L1ADJ.
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

static const char *TAG = "EX14_AUX_DCDC_ELOAD";

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
 * 2. Задача управления эквивалентом нагрузки eLOAD и коррекцией L1ADJ
 * ------------------------------------------------------------------------ */

static void eload_aux_test_task(void *pvParameters)
{
    bool eload_state = false;
    uint8_t wiper_val = 64;

    ESP_LOGI(TAG, "Запуск тестирования вспомогательного DC-DC (LM22675), eLOAD (CEN_L1) и DA4 L1ADG (0x2F)");

    while (1) {
        eload_state = !eload_state;
        ESP_LOGI(TAG, "--- [eLOAD / L1ADJ TEST] Эквивалент нагрузки: %s ---", eload_state ? "ВКЛ (CEN_L1=1)" : "ВЫКЛ (CEN_L1=0)");

        /* Управляем включением эквивалента нагрузки eLOAD через BSP (CEN_L1, P0.0) */
        (void)levisw_bsp_power_set(true, true, false, eload_state);

        /* Периодически изменяем уставку потенциометра DA4 (0x2F) для коррекции порога L1ADJ */
        wiper_val = (wiper_val == 64) ? 96 : 64;
        esp_err_t err_pot = levisw_bsp_pot_set_wiper(LEVISW_BUS_ULOAD, wiper_val);
        if (err_pot == ESP_OK) {
            ESP_LOGI(TAG, "[DA4 MCP4532] Уставка L1ADJ (0x2F) установлена в позицию wiper = %u", wiper_val);
        }

        if (eload_state) {
            app_led_set_rgb(0, 40, 50);  /* Бирюзовый: eLOAD активен */
        } else {
            app_led_set_rgb(30, 15, 0);  /* Янтарный: eLOAD отключен */
        }

        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 14: eLOAD, L1/V5 и порог L1ADJ");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 14: Power.SchDoc Лист 9)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, TCA9535, MCP4532) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());

    /* 3. Создание задачи тестирования eLOAD и L1ADJ */
    xTaskCreate(eload_aux_test_task, "eload_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 14 успешно запущен. Управление эквивалентом нагрузки и L1ADJ активно.");
}
