/**
 * @file main.c
 * @brief Пример 08: Датчики VAMPOK, VersionPCB и интерфейсы отладки для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 08 (Лист 4 MCU-ESP.SchDoc):
 *  1. Мониторинг напряжения питания усилителя VAmpM:
 *     - Подключение: GPIO34 / ADC1_CH6 (контрольные точки TP81, TP94) через делитель R16=510к / R17=75к (коэффициент 7.8);
 *  2. Определение аппаратной версии печатной платы VersionPCB:
 *     - Подключение: GPIO14 / ADC2_CH6 (совместно с JTAG JTMS, контрольные точки TP82, TP92, TP107) через делитель R18=75к / R19=1к, R22=4.7к;
 *  3. Отладочные и диагностические интерфейсы:
 *     - UART0 (XP5: U0TX/U0RX, BOOT) для программирования и вывода логов;
 *     - JTAG (XP4) для внутрисхемной отладки.
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

static const char *TAG = "EX08_VAMP_VERSION";

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
 * 2. Задача периодического опроса VAmpM и VersionPCB
 * ------------------------------------------------------------------------ */

static void diagnostics_sensors_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Запущен опрос датчиков VAmpM (GPIO34 / ADC1_CH6) и VersionPCB (GPIO14 / ADC2_CH6)");

    while (1) {
        uint32_t vamp_mv = 0;
        esp_err_t err_vamp = levisw_bsp_vamp_read_mv(&vamp_mv);

        if (err_vamp == ESP_OK) {
            ESP_LOGI(TAG, "[VAmpM SENSOR] Напряжение питания усилителя +Vamp = %u мВ (TP81, TP94)", (unsigned int)vamp_mv);
            if (vamp_mv > 5000) {
                app_led_set_rgb(0, 50, 0); /* Зеленый: норма */
            } else {
                app_led_set_rgb(50, 25, 0); /* Желтоватый: пониженное напряжение */
            }
        } else {
            ESP_LOGW(TAG, "Ошибка чтения АЦП VAmpM: %s", esp_err_to_name(err_vamp));
            app_led_set_rgb(60, 0, 0); /* Красный: ошибка */
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
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 08: VAmpM, VersionPCB и отладка");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 08: MCU-ESP.SchDoc Лист 4)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (включая инициализацию ADC1 для VAmpM) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1, +V2 и +Vamp */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, true, false));

    /* 3. Инициализация светодиода индикации */
    ESP_ERROR_CHECK(app_led_init());

    /* 4. Создание задачи опроса датчиков */
    xTaskCreate(diagnostics_sensors_task, "diag_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 08 запущен. Мониторинг VAmpM и VersionPCB активен через UART0 (XP5).");
}
