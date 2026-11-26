/**
 * @file main.c
 * @brief Пример 10: Узел защиты от «перевертышей» и смещения VOffset для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 10 (Лист 5/6 D8-OUT.SchDoc):
 *  1. Драйвер защиты и коммутации D1: MC33879APEKR2 (8-канальный ключ / VOffset);
 *  2. Управление по шине SPI2:
 *     - MOSI (GPIO7), MISO (GPIO9), SCLK (GPIO5);
 *     - Chip Select CS2 (GPIO8, CS-D, подтяжка RN19 10к);
 *     - Разрешение работы EN-DE (GPIO20, pull-down RN19 10к);
 *  3. На осмотренном экземпляре CORN13/RN13 не установлена:
 *     IN5 и IN6 MC33879 не подключены к GPIO27/GPIO21.
 *     Их уровни нужно проверить на самих выводах D1; GPIO21 не управляет IN6.
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

static const char *TAG = "EX10_MC33879_VOFFSET";

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
 * 2. Задача тестирования драйвера D1 MC33879 (VOffset / Защита)
 * ------------------------------------------------------------------------ */

static void mc33879_test_task(void *pvParameters)
{
    ESP_LOGW(TAG, "Тест MC33879: кадр 0xAA55 включает OUT1/OUT3/OUT5/OUT7; запускать только без абонентской линии!");

    /* IN5/IN6 не подключены к GPIO на осмотренной плате (CORN13/RN13 DNP).
     * GPIO21 нельзя использовать как управление IN6; проверять уровни на выводах D1.
     */

    uint32_t cycle = 0;

    while (1) {
        ESP_LOGI(TAG, "[MC33879 VOffset] Цикл %lu: Включение общего разрешения драйверов (EN-DE = 1, GPIO20)",
                 (unsigned long)cycle);

        /* 1. Включаем общую разрешающую линию EN-DE (GPIO20) */
        (void)levisw_bsp_drivers_enable(true);

        /* 2. Отправляем 16-битный SPI-кадр настройки/активации каналов MC33879 */
        uint16_t rx_frame = 0;
        uint16_t tx_frame = 0xAA55; /* OUT1/3/5/7 ON; open-load detect OUT8/6/4/2. Только стенд! */
        esp_err_t err = levisw_bsp_spi_mc33879_xfer(tx_frame, &rx_frame);
        if (err == ESP_OK) {
            ESP_LOGW(TAG, "[SPI MC33879] TX: 0x%04X, RX: 0x%04X (включены OUT1/3/5/7; VOffset этим тестом не подтверждён)", tx_frame, rx_frame);
            app_led_set_rgb(20, 50, 20); /* Зеленоватый */
        } else {
            ESP_LOGW(TAG, "Ошибка SPI обмена с MC33879: %s", esp_err_to_name(err));
            app_led_set_rgb(60, 0, 0);   /* Красный */
        }

        vTaskDelay(pdMS_TO_TICKS(2000));

        /* Выключение */
        (void)levisw_bsp_drivers_enable(false);
        app_led_set_rgb(10, 10, 30); /* Синеватый */

        cycle++;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 10: Защита и VOffset MC33879");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 10: D8-OUT.SchDoc / Лист 5-6)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, SPI2, TCA9535) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 3. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());

    /* 4. Создание задачи тестирования MC33879 */
    xTaskCreate(mc33879_test_task, "mc33_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 10 успешно запущен. Узел защиты от перевертышей и VOffset активен.");
}
