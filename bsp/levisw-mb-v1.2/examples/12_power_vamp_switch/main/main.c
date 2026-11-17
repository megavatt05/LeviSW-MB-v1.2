/**
 * @file main.c
 * @brief Пример 12: Входная защита питания и ключ усилителя +Vamp для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 12 (Лист 7 Power.SchDoc):
 *  1. Входная защита питающих цепей от переполюсовки и всплесков;
 *  2. Ключ коммутации питания аудиоусилителя +Vamp (транзисторы VT4 / VT5);
 *  3. Управление сигналом VAMPON через расширитель DD3 TCA9535 (бит P0.4, контрольные точки TP63, TP153);
 *  4. Измерение фактического напряжения питания +Vamp через датчик VAmpM (GPIO34 / ADC1_CH6, TP81, TP94).
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

static const char *TAG = "EX12_POWER_VAMP";

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
 * 2. Задача тестирования ключа питания +Vamp и мониторинга VAmpM
 * ------------------------------------------------------------------------ */

static void vamp_power_test_task(void *pvParameters)
{
    bool vamp_state = false;

    ESP_LOGI(TAG, "Запуск теста ключа питания +Vamp (VT4/VT5, VAMPON P0.4) и мониторинга VAmpM (TP81)");

    while (1) {
        vamp_state = !vamp_state;
        ESP_LOGI(TAG, "--- [VAMP POWER TEST] Переключение ключа +Vamp: %s ---", vamp_state ? "ВКЛ (VAMPON=1)" : "ВЫКЛ (VAMPON=0)");

        /* Управляем питанием +Vamp через функцию BSP (оставляя +V1 и +V2 включенными) */
        esp_err_t err = levisw_bsp_power_set(true, true, vamp_state, false);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Ошибка установки питания +Vamp: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(500));

        /* Считываем фактическое напряжение +Vamp через АЦП VAmpM (GPIO34 / ADC1_CH6) */
        uint32_t vamp_mv = 0;
        if (levisw_bsp_vamp_read_mv(&vamp_mv) == ESP_OK) {
            ESP_LOGI(TAG, "[VAmpM TELEMETRY] Фактическое напряжение +Vamp = %u мВ (TP81, TP94)", (unsigned int)vamp_mv);
        }

        if (vamp_state) {
            app_led_set_rgb(0, 60, 0);  /* Зеленый: питание +Vamp включено */
        } else {
            app_led_set_rgb(40, 20, 0); /* Янтарный: питание выключено */
        }

        vTaskDelay(pdMS_TO_TICKS(4500));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 12: Защита питания и ключ +Vamp");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 12: Power.SchDoc Лист 7)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, TCA9535, АЦП) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());

    /* 3. Создание задачи тестирования питания +Vamp */
    xTaskCreate(vamp_power_test_task, "vamp_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 12 успешно запущен. Управление ключом +Vamp и мониторинг активны.");
}
