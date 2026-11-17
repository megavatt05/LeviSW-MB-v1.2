/**
 * @file main.c
 * @brief Пример 04: Узел детекции статуса трубок LU1 / LU2 для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 04 (Лист 3 Com-485-LU-1W.SchDoc):
 *  1. Формирователь дискретных сигналов детекции трубок:
 *     - DD1 (74LVC2G17GW,125) — двойной триггер Шмитта;
 *     - Входные высокоомные делители R2, R3 (510 кОм) и R7, R8 (1 МОм);
 *     - Защитные диоды VD1, VD2 (300 мА / 40 В) для ограничения напряжения до шины +3.3В;
 *     - Фильтрующие конденсаторы C3, C4 (18 пФ) и C6 (100 нФ) для подавления дребезга и помех;
 *  2. Подключение к микроконтроллеру DD4 (ESP32-PICO-V3):
 *     - LU1 -> GPIO36 (ADC1_CH0 / I36, контрольные точки TP7, TP45, TP50);
 *     - LU2 -> GPIO37 (ADC1_CH1 / I37, контрольные точки TP6, TP46, TP51);
 *  3. Локальная индикация LED1 (WS2812C, GPIO23):
 *     - Зелёный: обе абонентские трубки уложены (Standby);
 *     - Желтый/Оранжевый: зафиксировано снятие трубки на линии LU1 или LU2.
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

static const char *TAG = "EX04_LU_DETECTOR";

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
 * 2. Задача мониторинга состояния абонентских трубок LU1 / LU2
 * ------------------------------------------------------------------------ */

static void lu_monitoring_task(void *pvParameters)
{
    bool prev_lu1 = false;
    bool prev_lu2 = false;
    uint32_t stable_count_1 = 0;
    uint32_t stable_count_2 = 0;

    ESP_LOGI(TAG, "Запущен постоянный опрос линий детекции трубoк LU1 (GPIO36, TP7) и LU2 (GPIO37, TP6)");

    while (1) {
        /* Чтение сырых уровней с триггера Шмитта DD1 74LVC2G17 через BSP */
        bool raw_lu1 = levisw_bsp_lu1_read();
        bool raw_lu2 = levisw_bsp_lu2_read();

        /* Программный фильтр дребезга контактов (требуется 3 подтверждения подряд по 20 мс) */
        if (raw_lu1 == prev_lu1) {
            if (stable_count_1 < 5) stable_count_1++;
        } else {
            stable_count_1 = 0;
            prev_lu1 = raw_lu1;
        }

        if (raw_lu2 == prev_lu2) {
            if (stable_count_2 < 5) stable_count_2++;
        } else {
            stable_count_2 = 0;
            prev_lu2 = raw_lu2;
        }

        /* Вывод в лог при стабильном изменении состояния */
        if (stable_count_1 == 3 || stable_count_2 == 3) {
            ESP_LOGI(TAG, "[LU STATUS] LU1 (Линия 1, TP7/TP45) = %s | LU2 (Линия 2, TP6/TP46) = %s",
                     prev_lu1 ? "СНЯТА (Off-Hook)" : "УЛОЖЕНА (On-Hook)",
                     prev_lu2 ? "СНЯТА (Off-Hook)" : "УЛОЖЕНА (On-Hook)");

            /* Индикация: если хотя бы одна трубка снята — желтый цвет, иначе зелёный */
            if (prev_lu1 || prev_lu2) {
                app_led_set_rgb(90, 45, 0); /* Янтарный / Оранжевый */
            } else {
                app_led_set_rgb(0, 50, 0);  /* Зелёный (Standby) */
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 04: Детекция трубок LU1 / LU2");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 04: Com-485-LU-1W.SchDoc Лист 3)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация BSP платы (I2C, GPIO, АЦП, TCA9535) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 3. Инициализация светодиода индикации */
    ESP_ERROR_CHECK(app_led_init());
    app_led_set_rgb(0, 50, 0); /* Старт: зеленый */

    /* 4. Создание задачи мониторинга LU1 / LU2 */
    xTaskCreate(lu_monitoring_task, "lu_task", 3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 04 запущен. Ожидание сигналов снятия/положения трубок на XP1:6/7.");
}
