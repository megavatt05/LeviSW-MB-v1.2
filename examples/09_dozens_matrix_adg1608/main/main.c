/**
 * @file main.c
 * @brief Пример 09: Проверка аналоговых каналов AMP1..AMP8 через ADG1608.
 * @note Имя каталога историческое: D0..D7 через MC33879 этот тест НЕ переключает.
 *
 * По листу D8 DA1 ADG1608 выбирает маршрут +LINE -> AMP1..AMP8 -> XS3/LeviAMP.
 * Выходы D0..D7 на XP2 формируются MC33879 и здесь не переключаются.
 * Управление ADG1608 выполняется через DD3 TCA9535 (SW-A0..SW-A2, SW-EN).
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

static const char *TAG = "EX09_ADG1608_AMP";

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
 * 2. Задача тестирования аналоговых каналов AMP1..AMP8 через ADG1608
 * ------------------------------------------------------------------------ */

static void amp_channel_sweep_task(void *pvParameters)
{
    uint8_t current_channel = 0;

    ESP_LOGW(TAG, "ТЕСТ ADG1608 перебирает AMP1..AMP8, а не D0..D7; не запускать на активной абонентской линии!");
    ESP_LOGI(TAG, "Сканирование аналоговых маршрутов ADG1608 S1..S8 -> AMP1..AMP8 / XS3");

    while (1) {
        ESP_LOGI(TAG, "[AMP ROUTE] ADG1608 S%u -> AMP%u",
                 current_channel + 1U, current_channel + 1U);

        /* Выбираем аналоговый AMP-маршрут. D0..D7 этим вызовом не переключаются. */
        esp_err_t err = levisw_bsp_adg1608_select(current_channel, true);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Ошибка переключения ADG1608: %s", esp_err_to_name(err));
        }

        /* Индикация цветом текущего десятка */
        switch (current_channel % 4) {
        case 0: app_led_set_rgb(40, 0, 40); break; /* Пурпурный */
        case 1: app_led_set_rgb(0, 40, 40); break; /* Голубой */
        case 2: app_led_set_rgb(40, 40, 0); break; /* Желтый */
        case 3: app_led_set_rgb(20, 20, 20); break;/* Белый */
        }

        current_channel = (current_channel + 1U) % LEVISW_ADG1608_CHANNELS_COUNT;
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 09: ADG1608 AMP routes");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 09: D8-OUT.SchDoc Лист 5)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, TCA9535, АЦП) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 3. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());

    /* 4. Создание задачи сканирования десятков */
    xTaskCreate(amp_channel_sweep_task, "amp_route_task", 4096, NULL, 5, NULL);

    ESP_LOGW(TAG, "Пример 09 запущен: сканируются AMP1..AMP8. Матрица D0..D7 не тестируется.");
}
