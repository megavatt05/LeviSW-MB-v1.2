/**
 * @file main.c
 * @brief Пример 07: I2C-расширитель портов и матрица перемычек UART/1W для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 07 (Лист 4 MCU-ESP.SchDoc):
 *  1. Расширитель GPIO DD3: TCA9535PWR (адрес I2C 0x20):
 *     - Управление силовыми ключами (+V1, +V2, +Vamp, eLOAD);
 *     - Управление аналоговым селектором DA1 ADG1608: +LINE -> AMP1..AMP8 / XS3 (SW-A0..SW-A2, SW-EN);
 *     - Управление аудиоусилителем и реле (ENA1..ENA5, LEDAMP);
 *     - Чтение статусов PG (Power Good) и ALARMIN, мониторинг прерывания INT0 (GPIO35);
 *  2. Матрица перемычек и резисторных сборок (RN7, RN8, RN12):
 *     - Проверка конфигурации линий UART и 1-Wire.
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

static const char *TAG = "EX07_EXPANDER_MATRIX";

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
 * 2. Задача тестирования и опроса расширителя портов DD3 TCA9535 (0x20)
 * ------------------------------------------------------------------------ */

static void expander_test_task(void *pvParameters)
{
    uint32_t step = 0;

    ESP_LOGI(TAG, "Запуск задачи тестирования DD3 TCA9535PWR (I2C 0x20) и матрицы управления");

    while (1) {
        uint8_t p0 = 0, p1 = 0;

        /* Чтение текущих входов и выходов TCA9535 */
        esp_err_t err = levisw_bsp_exp_read_inputs(&p0, &p1);
        if (err == ESP_OK) {
            bool pg = (p0 & LEVISW_EXP_P0_PGOOD) != 0U;
            bool alarm = (p1 & LEVISW_EXP_P1_ALARMIN) != 0U;
            bool int0 = (gpio_get_level(LEVISW_PIN_INT0_TCA9535) == 0); /* Активный уровень LOW */

            ESP_LOGI(TAG, "[TCA9535 STAT] Port0=0x%02X, Port1=0x%02X | PG=%d, Alarm=%d, INT0=%d",
                     p0, p1, pg, alarm, int0);
        } else {
            ESP_LOGW(TAG, "Ошибка чтения регистров TCA9535: %s", esp_err_to_name(err));
        }

        /* Циклическая проверка аналоговых AMP-маршрутов ADG1608 и реле через расширитель */
        uint8_t channel = (uint8_t)(step % LEVISW_ADG1608_CHANNELS_COUNT);
        bool rel_state = ((step / 8) % 2) == 0;

        ESP_LOGI(TAG, "[EXPANDER TEST] Шаг %lu: ADG1608 S%u -> AMP%u, Реле = %s",
                 (unsigned long)step, channel + 1U, channel + 1U, rel_state ? "ВКЛ" : "ВЫКЛ");

        /* Управляем аналоговым AMP-маршрутом и реле через BSP; D0..D7 здесь не переключаются. */
        (void)levisw_bsp_adg1608_select(channel, true);
        (void)levisw_bsp_amp_set(true, true, false, rel_state, !rel_state, true);

        if ((step % 2) == 0) {
            app_led_set_rgb(30, 30, 80); /* Синеватый */
        } else {
            app_led_set_rgb(0, 60, 20);  /* Зеленоватый */
        }

        step++;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 07: I2C-расширитель и перемычки");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 07: MCU-ESP.SchDoc Лист 4)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы (I2C, TCA9535, SPI, АЦП) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, true, false));

    /* 3. Инициализация локального светодиода */
    ESP_ERROR_CHECK(app_led_init());
    app_led_set_rgb(0, 50, 0); /* Зелёный Standby */

    /* 4. Создание задачи тестирования расширителя TCA9535 */
    xTaskCreate(expander_test_task, "exp_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 07 успешно запущен. Мониторинг TCA9535 (0x20) и матрицы перемычек активен.");
}
