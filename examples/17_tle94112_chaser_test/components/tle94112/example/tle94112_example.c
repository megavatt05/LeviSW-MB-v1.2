/*
 * SPDX-FileCopyrightText: 2026 Albert
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "tle94112.h"
#include "tle94112_example.h"

static const char *TAG = "tle94112_example";

// Конфигурация выводов
#define TLE_SPI_HOST   SPI2_HOST
#define TLE_GPIO_MOSI  7
#define TLE_GPIO_MISO  9
#define TLE_GPIO_SCLK  5
#define TLE_GPIO_CS    10
#define TLE_GPIO_EN    20

#define START_DELAY_MS             2000
#define SWEEP_STATE_DELAY_MS       500
#define RANDOM_TOGGLE_DELAY_MS     800
#define RANDOM_STEPS_COUNT         20

// Глобальный дескриптор драйвера
static tle94112_handle_t s_tle_handle = NULL;

static void sweep_single_port_states(void)
{
    ESP_LOGI(TAG, "[TLE_SWEEP] PROGRAM: TLE94112_SWEEP");
    ESP_LOGI(TAG, "[TLE_SWEEP] Sequence: UP -> DOWN -> FLOATING");

    for (int hb = TLE_HB1; hb <= TLE_HB12; ++hb) {
        // Перевести все полумосты в FLOATING
        esp_err_t ret = tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[TLE_SWEEP] Failed to set FLOATING: %s", esp_err_to_name(ret));
            continue;
        }

        // Включить HIGH_SIDE
        ESP_LOGI(TAG, "[TLE_SWEEP] HB%d -> UP", hb + 1);
        ret = tle94112_set_hb_state(s_tle_handle, (tle_hb_idx_t)hb, TLE_HB_HS_ON);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[TLE_SWEEP] Failed to set HB%d UP: %s", hb + 1, esp_err_to_name(ret));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(SWEEP_STATE_DELAY_MS));

        // Включить LOW_SIDE
        ESP_LOGI(TAG, "[TLE_SWEEP] HB%d -> DOWN", hb + 1);
        ret = tle94112_set_hb_state(s_tle_handle, (tle_hb_idx_t)hb, TLE_HB_LS_ON);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[TLE_SWEEP] Failed to set HB%d DOWN: %s", hb + 1, esp_err_to_name(ret));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(SWEEP_STATE_DELAY_MS));

        // Перевести выходы в FLOATING
        ESP_LOGI(TAG, "[TLE_SWEEP] HB%d -> FLOATING", hb + 1);
        ret = tle94112_set_hb_state(s_tle_handle, (tle_hb_idx_t)hb, TLE_HB_FLOATING);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[TLE_SWEEP] Failed to set HB%d FLOATING: %s", hb + 1, esp_err_to_name(ret));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(SWEEP_STATE_DELAY_MS));
    }
}

static void random_music_mode(void)
{
    ESP_LOGI(TAG, "[TLE_RANDOM_MUSIC] PROGRAM: TLE94112_RANDOM_MUSIC");
    ESP_LOGI(TAG, "[TLE_RANDOM_MUSIC] Random port and state changes");

    for (int i = 0; i < RANDOM_STEPS_COUNT; ++i) {
        tle_hb_idx_t hb = (tle_hb_idx_t)(esp_random() % 12U);
        uint32_t raw_state = esp_random() % 3U;
        tle_hb_state_t state = (raw_state == 0U) ? TLE_HB_HS_ON :
                               (raw_state == 1U) ? TLE_HB_LS_ON : TLE_HB_FLOATING;

        esp_err_t ret = tle94112_set_hb_state(s_tle_handle, hb, state);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[TLE_RANDOM_MUSIC] Failed step %d: %s", i + 1, esp_err_to_name(ret));
            continue;
        }
        ESP_LOGI(TAG, "[TLE_RANDOM_MUSIC] step=%d HB%d state=%d",
                 i + 1, (int)hb + 1, (unsigned)state);

        vTaskDelay(pdMS_TO_TICKS(RANDOM_TOGGLE_DELAY_MS));
    }
}

static void tle94112_example_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "[TLE_BOOT] PROGRAM: TLE94112_DIAG_DEMO");

    // 1. Инициализация шины SPI
    spi_bus_config_t buscfg = {
        .mosi_io_num = TLE_GPIO_MOSI,
        .miso_io_num = TLE_GPIO_MISO,
        .sclk_io_num = TLE_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    esp_err_t ret = spi_bus_initialize(TLE_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[TLE_BOOT] SPI bus init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    // 2. Настройка TLE94112
    tle94112_config_t cfg = {
        .spi_host = TLE_SPI_HOST,
        .gpio_cs = TLE_GPIO_CS,
        .gpio_en = TLE_GPIO_EN,
        .spi_freq_hz = 1000000  // 1 МГц
    };

    // 3. Инициализация драйвера
    ESP_LOGI(TAG, "[TLE_BOOT] Init TLE94112");
    s_tle_handle = tle94112_init(&cfg);
    if (!s_tle_handle) {
        ESP_LOGE(TAG, "[TLE_BOOT] Initialization failed");
        spi_bus_free(TLE_SPI_HOST);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[TLE_BOOT] TLE94112 initialized successfully");

    // 4. Очистка ошибок после инициализации
    ret = tle94112_clear_errors(s_tle_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to clear initial errors");
    }

    // 5. Запуск диагностики
    tle94112_diagnose(s_tle_handle);

    ESP_LOGI(TAG, "[TLE_BOOT] Wait %d ms", START_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(START_DELAY_MS));

    // 6. Выполнение примеров
    sweep_single_port_states();
    random_music_mode();

    // 7. Перевод всех полумостов в FLOATING
    ret = tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[TLE_DONE] Failed to set FLOATING: %s", esp_err_to_name(ret));
    }

    // 8. Итоговая диагностика
    tle94112_diagnose(s_tle_handle);

    // 9. Очистка ошибок
    ret = tle94112_clear_errors(s_tle_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[TLE_DONE] Failed to clear errors: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "[TLE_DONE] Demo finished, entering idle loop");

    // 10. Цикл ожидания с периодической диагностикой
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  // 30 секунд

        // Периодическая диагностика
        tle94112_diagnose(s_tle_handle);

        // Очистить ошибки, если они есть
        tle94112_clear_errors(s_tle_handle);
    }
}

esp_err_t tle94112_example_start(const char *task_name,
                                 uint32_t stack_size,
                                 UBaseType_t priority,
                                 BaseType_t core_id)
{
    const char *name = task_name ? task_name : "tle94112_example";
    uint32_t stack = stack_size ? stack_size : 4096;
    UBaseType_t prio = priority ? priority : 3;

    BaseType_t ok = xTaskCreatePinnedToCore(
        tle94112_example_task,
        name,
        stack,
        NULL,
        prio,
        NULL,
        core_id
    );

    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}
