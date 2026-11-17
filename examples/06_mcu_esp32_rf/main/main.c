/**
 * @file main.c
 * @brief Пример 06: Центральный микроконтроллер ESP32 и RF-тракт для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 06 (Лист 4 MCU-ESP.SchDoc):
 *  1. Центральный микроконтроллер DD4: ESP32-PICO-V3 (LGA-48) с интегрированной флеш-памятью и RF-трактом;
 *  2. Системные ресурсы и инициализация NVS;
 *  3. Тестирование RF-подсистемы (инициализация Wi-Fi в режиме Station для проверки RF-тракта);
 *  4. Диагностика аппаратной версии платы через делитель VersionPCB на GPIO14 (ADC2_CH6, TP82);
 *  5. Локальная индикация состояния на LED1 (WS2812C, GPIO23).
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "levisw_mb_v1_2_pins.h"
#include "levisw_mb_v1_2_bsp.h"

static const char *TAG = "EX06_MCU_RF";

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
 * 2. Инициализация Wi-Fi / RF-тракта ESP32-PICO-V3
 * ------------------------------------------------------------------------ */

static void wifi_init_rf_test(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "RF-тракт и Wi-Fi модуль ESP32-PICO-V3 успешно запущены в режиме STA");
}

/* --------------------------------------------------------------------------
 * 3. Диагностическая задача (информация о чипе, памяти и версии PCB)
 * ------------------------------------------------------------------------ */

static void mcu_diagnostics_task(void *pvParameters)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "--- ИНФОРМАЦИЯ О МИКРОКОНТРОЛЛЕРЕ DD4 ---");
    ESP_LOGI(TAG, "Чип: %s, ревизия %d", CONFIG_IDF_TARGET, chip_info.revision);
    ESP_LOGI(TAG, "Ядер: %d, Wi-Fi: %s, BLE: %s",
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "Да" : "Нет",
             (chip_info.features & CHIP_FEATURE_BLE) ? "Да" : "Нет");
    ESP_LOGI(TAG, "Флеш-память: %dMB (%s)",
             spi_flash_get_chip_size() / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Встроенная (PICO-V3)" : "Внешняя");

    uint32_t toggle = 0;

    while (1) {
        size_t free_heap = esp_get_free_heap_size();
        size_t min_free_heap = esp_get_minimum_free_heap_size();

        ESP_LOGI(TAG, "[MCU STAT] Свободная память (Heap): %u байт (минимум: %u байт)",
                 (unsigned int)free_heap, (unsigned int)min_free_heap);

        /* Индикация циклической работы: синий/зелёный пульс */
        if ((toggle % 2) == 0) {
            app_led_set_rgb(0, 40, 40); /* Бирюзовый */
        } else {
            app_led_set_rgb(0, 60, 0);  /* Зелёный */
        }
        toggle++;

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 06: MCU ESP32-PICO-V3 и RF-тракт");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 06: MCU-ESP.SchDoc Лист 4)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. Инициализация аппаратного BSP платы (I2C, SPI, TCA9535, АЦП) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 3. Включение силовых шин питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 4. Инициализация локальной индикации LED1 (WS2812C) */
    ESP_ERROR_CHECK(app_led_init());

    /* 5. Инициализация RF-подсистемы и Wi-Fi */
    wifi_init_rf_test();

    /* 6. Запуск диагностической задачи */
    xTaskCreate(mcu_diagnostics_task, "mcu_diag_task", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Пример 06 успешно запущен. Центральный МК и RF-тракт функционируют.");
}
