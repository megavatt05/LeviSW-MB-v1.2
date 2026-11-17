/**
 * @file main.c
 * @brief Пример 09: Коммутатор десятков и диодная защита линий D0–D7 для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 09 (Лист 5 D8-OUT.SchDoc):
 *  1. Коммутатор десятков DA1: ADG1608BR — 8-канальный аналоговый мультиплексор;
 *  2. Клеммник десятков XP2:1..8 (линии D0..D7, контрольные точки TP27..TP34);
 *  3. Диодная защитная цепь линий D0..D7 от высоковольтных выбросов и наводок;
 *  4. Управление коммутатором через расширитель DD3 TCA9535 (биты SW-A0..SW-A2, SW-EN).
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

static const char *TAG = "EX09_DOZENS_MATRIX";

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
 * 2. Задача тестирования коммутатора десятков D0..D7 (ADG1608)
 * ------------------------------------------------------------------------ */

static void dozens_sweep_task(void *pvParameters)
{
    uint8_t current_dozen = 0;

    ESP_LOGI(TAG, "Запуск сканирования и коммутации десятков D0..D7 (XP2:1..8, TP27..TP34) через DA1 ADG1608");

    while (1) {
        ESP_LOGI(TAG, "[MATRIX SW] Выбор линии десятков D%u (Клемма XP2:%u, TP%u) | Диодная защита активна",
                 current_dozen, current_dozen + 1, 27 + current_dozen);

        /* Коммутируем десяток на мультиплексоре ADG1608 с разрешением (SW-EN = 1) */
        esp_err_t err = levisw_bsp_adg1608_select(current_dozen, true);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Ошибка переключения ADG1608: %s", esp_err_to_name(err));
        }

        /* Индикация цветом текущего десятка */
        switch (current_dozen % 4) {
        case 0: app_led_set_rgb(40, 0, 40); break; /* Пурпурный */
        case 1: app_led_set_rgb(0, 40, 40); break; /* Голубой */
        case 2: app_led_set_rgb(40, 40, 0); break; /* Желтый */
        case 3: app_led_set_rgb(20, 20, 20); break;/* Белый */
        }

        current_dozen = (current_dozen + 1) % LEVISW_MATRIX_DOZENS_COUNT;
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 09: Коммутатор десятков D0-D7");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 09: D8-OUT.SchDoc Лист 5)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, TCA9535, АЦП) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 3. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());

    /* 4. Создание задачи сканирования десятков */
    xTaskCreate(dozens_sweep_task, "dozens_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 09 успешно запущен. Сканирование матрицы десятков D0..D7 активно.");
}
