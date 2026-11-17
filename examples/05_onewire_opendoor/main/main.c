/**
 * @file main.c
 * @brief Пример 05: Каскад 1-Wire и управления открытием двери для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратных цепей 1-Wire и управления замком (Лист 3 Com-485-LU-1W.SchDoc и Лист 2):
 *  1. Интерфейс 1-Wire / OpenDoor через установленную сборку RN12 (GPIO-1W):
 *     - 1WRx -> GPIO12 / MTDI (контрольные точки TP57, TP90; учтён pull-down RN6 10k для безопасности загрузки);
 *     - 1WTx -> GPIO15 / MTDO (контрольная точка TP93);
 *  2. Узел аппаратного открытия двери (OpenD):
 *     - Перемычка R1 (0 Ом) объединяет линию OpenD с выходом SW-OUT2 (полумост PO2 драйвера D2 TLE94112EL);
 *  3. Индикация LED1 (WS2812C, GPIO23):
 *     - Зелёный: дежурный режим;
 *     - Бирюзовая вспышка: активный импульс открытия замка (OpenD).
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

static const char *TAG = "EX05_1WIRE_OPENDOOR";

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
 * 2. Управление открытием замка через R1 (0 Ом) -> SW-OUT2 (PO2 TLE94112EL)
 * ------------------------------------------------------------------------ */

static esp_err_t execute_door_open_sequence(void)
{
    ESP_LOGI(TAG, ">>> [OPEN DOOR] Запуск последовательности отпирания замка (OpenD / XP1:9)");

    /* Установка синего/бирюзового цвета индикатора во время открытия */
    app_led_set_rgb(50, 255, 200);

    /*
     * Шаг 1: Разрешаем работу SPI-драйверов (EN-DE = 1, GPIO20)
     */
    ESP_RETURN_ON_ERROR(levisw_bsp_drivers_enable(true), TAG, "Ошибка включения драйверов D1/D2");

    /*
     * Шаг 2: Включаем полумост PO2 (LEVISW_TLE_HB_SW_OUT2), соединённый через R1 (0 Ом) с OpenD.
     * Отправляем кадр управления для замыкания полумоста PO2 на питание/землю.
     */
    uint16_t rx_frame = 0;
    /* Пример кадра управления TLE94112EL: активация выхода PO2 (полумост 2) в режим High-Side */
    uint16_t tx_frame = 0x8201; /* Условная команда активации PO2 */
    ESP_RETURN_ON_ERROR(levisw_bsp_spi_tle94112_xfer(tx_frame, &rx_frame), TAG, "Ошибка SPI обмена с TLE94112");
    ESP_LOGI(TAG, "[SPI TLE94112] Отправлено: 0x%04X, Получено: 0x%04X (Замок открыт)", tx_frame, rx_frame);

    /* Выдерживаем импульс отпирания 500 мс */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Шаг 3: Отключаем полумост и перевод драйверов в безопасный режим (EN-DE = 0) */
    tx_frame = 0x8200; /* Команда деактивации PO2 */
    (void)levisw_bsp_spi_tle94112_xfer(tx_frame, &rx_frame);
    ESP_RETURN_ON_ERROR(levisw_bsp_drivers_enable(false), TAG, "Ошибка выключения драйверов D1/D2");

    ESP_LOGI(TAG, "<<< [OPEN DOOR] Импульс отпирания завершён, замок заблокирован.");
    app_led_set_rgb(0, 50, 0); /* Возврат к зеленому Standby */
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * 3. Инициализация и опрос 1-Wire шины (GPIO12 / 1WRx, GPIO15 / 1WTx)
 * ------------------------------------------------------------------------ */

static void onewire_polling_task(void *pvParameters)
{
    /* Настройка GPIO12 (1WRx) и GPIO15 (1WTx) */
    gpio_config_t ow_rx_cfg = {
        .pin_bit_mask = (1ULL << LEVISW_PIN_1W_RX),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&ow_rx_cfg);

    ESP_LOGI(TAG, "Интерфейс 1-Wire инициализирован (1WRx=GPIO12/MTDI [TP57/TP90], 1WTx=GPIO15/MTDO [TP93])");

    uint32_t cycle_count = 0;

    while (1) {
        bool ow_level = (gpio_get_level(LEVISW_PIN_1W_RX) != 0);
        ESP_LOGD(TAG, "[1-Wire Bus] Состояние линии 1WRx (GPIO12) = %d", ow_level);

        /* Каждые 10 секунд имитируем тестовый цикл отпирания двери */
        if (cycle_count > 0 && (cycle_count % 10) == 0) {
            (void)execute_door_open_sequence();
        }

        cycle_count++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 05: 1-Wire и открытие двери");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 05: Com-485-LU-1W.SchDoc / Switcher)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация BSP платы (I2C, SPI, TCA9535, АЦП) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* 3. Инициализация светодиода индикации */
    ESP_ERROR_CHECK(app_led_init());
    app_led_set_rgb(0, 50, 0); /* Зелёный Standby */

    /* 4. Создание задачи 1-Wire и управления замком */
    xTaskCreate(onewire_polling_task, "ow_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 05 успешно запущен. Мониторинг 1-Wire и периодический тест замка активны.");
}
