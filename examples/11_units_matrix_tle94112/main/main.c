/**
 * @file main.c
 * @brief Пример 11: Коммутатор единиц E0–E9 и фильтр возврата -LINE для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 11 (Лист 6 E10-OUT.SchDoc):
 *  1. Драйвер единиц D2: TLE94112EL — 12-полумостовой интеллектуальный SPI-коммутатор;
 *  2. Клеммник единиц XP3:1..10 (линии E0..E9, контрольные точки TP35..TP44);
 *  3. Нелинейное сопоставление цифры единиц (0..9) полумостам PO1..PO12 (использование BSP маппинга);
 *  4. Фильтр возврата разговорной линии (-LINE) и управление через SPI.
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

static const char *TAG = "EX11_UNITS_MATRIX";

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
 * 2. Задача тестирования коммутатора единиц E0..E9 (TLE94112EL)
 * ------------------------------------------------------------------------ */

static void units_matrix_sweep_task(void *pvParameters)
{
    uint8_t unit_digit = 0;

    ESP_LOGI(TAG, "Запуск сканирования и коммутации единиц E0..E9 (XP3:1..10, TP35..TP44) через D2 TLE94112EL");

    while (1) {
        /* Получаем номер полумоста TLE94112 (1..12) для цифры единиц 0..9 используя BSP */
        uint8_t hb = levisw_bsp_unit_to_tle_hb(unit_digit);

        ESP_LOGI(TAG, "[UNITS MATRIX] Единица E%u (Клеммник XP3:%u, TP%u) -> Полумост PO%u драйвера TLE94112",
                 unit_digit, unit_digit + 1, 35 + unit_digit, hb);

        /* Включаем разрешение драйверов (EN-DE = 1, GPIO20) */
        (void)levisw_bsp_drivers_enable(true);

        /* Отправляем SPI-кадр активации соответствующего полумоста */
        uint16_t rx_frame = 0;
        uint16_t tx_frame = (uint16_t)(0x8000 | (hb << 8) | 0x01); /* Команда включения High-Side */
        esp_err_t err = levisw_bsp_spi_tle94112_xfer(tx_frame, &rx_frame);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[SPI TLE94112] Кадр отправлен успешно (RX: 0x%04X)", rx_frame);
        }

        /* Индикация цветом */
        switch (unit_digit % 3) {
        case 0: app_led_set_rgb(0, 40, 40); break; /* Бирюзовый */
        case 1: app_led_set_rgb(40, 20, 0); break; /* Оранжевый */
        case 2: app_led_set_rgb(20, 0, 40); break; /* Фиолетовый */
        }

        unit_digit = (unit_digit + 1) % LEVISW_MATRIX_UNITS_COUNT;
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 11: Коммутатор единиц E0-E9");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 11: E10-OUT.SchDoc Лист 6)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, SPI2, TCA9535) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 3. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());

    /* 4. Создание задачи сканирования единиц */
    xTaskCreate(units_matrix_sweep_task, "units_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 11 успешно запущен. Сканирование матрицы единиц E0..E9 активно.");
}
