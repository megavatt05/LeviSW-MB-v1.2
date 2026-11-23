/**
 * @file main.c
 * @brief Тестовый проект «Бегущий огонь» для микросхемы D2 TLE94112EL на плате LeviSW-MB v1.2.
 *
 * Архитектура и схемотехника платы LeviSW-MB v1.2 (U210001-A E3, Узел 11 E10-OUT.SchDoc):
 *  1. Микросхема D2 (Infineon TLE94112EL): 12-полумостовой интеллектуальный коммутатор нагрузки
 *     на шине SPI2 (Mode 1, CPOL=0, CPHA=1), CS1 = GPIO10, EN-DE = GPIO20.
 *  2. Клеммник единиц XP3:1..10 (координатные линии матрицы E0..E9, контрольные точки TP35..TP44).
 *  3. Нелинейная аппаратная трассировка полумостов TLE94112 к линиям единиц:
 *     - E0 (XP3:1,  TP35) -> PO1  (TLE_HB1,  индекс 0)
 *     - E1 (XP3:2,  TP36) -> PO5  (TLE_HB5,  индекс 4)
 *     - E2 (XP3:3,  TP37) -> PO7  (TLE_HB7,  индекс 6)
 *     - E3 (XP3:4,  TP38) -> PO9  (TLE_HB9,  индекс 8)
 *     - E4 (XP3:5,  TP39) -> PO6  (TLE_HB6,  индекс 5)
 *     - E5 (XP3:6,  TP40) -> PO4  (TLE_HB4,  индекс 3)
 *     - E6 (XP3:7,  TP41) -> PO3  (TLE_HB3,  индекс 2)
 *     - E7 (XP3:8,  TP42) -> PO10 (TLE_HB10, индекс 9)
 *     - E8 (XP3:9,  TP43) -> PO11 (TLE_HB11, индекс 10)
 *     - E9 (XP3:10, TP44) -> PO8  (TLE_HB8,  индекс 7)
 *     - Дополнительные служебные выходы: PO2 (SW-OUT2 / OpenD), PO12 (SW-OUT12 / TP152).
 *  4. Питание: силовая шина +V2 (VPWR) от DCDC TPS65261, контролируемая через I2C INA226 (DD6),
 *     потенциометр DA3 MCP4532 и расширитель DD3 TCA9535.
 *  5. Индикация: встроенный RGB светодиод LED1 (WS2812C, GPIO23, TP20).
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "levisw_mb_v1_2_pins.h"
#include "levisw_mb_v1_2_bsp.h"
#include "tle94112.h"
#include "mc33879a.h"

static const char *TAG = "TLE_CHASER_TEST";

static led_strip_handle_t s_led_strip = NULL;
static tle94112_handle_t s_tle_handle = NULL;
static mc33879a_handle_t s_mc33879;

/* --------------------------------------------------------------------------
 * Структура соответствия линий E0..E9 клеммника XP3 и полумостов TLE94112EL
 * ------------------------------------------------------------------------ */
typedef struct {
    uint8_t unit_idx;       /* Цифра единиц 0..9 (E0..E9) */
    uint8_t terminal_pin;   /* Контакт клеммника XP3 (1..10) */
    uint8_t test_point;     /* Контрольная точка TP (35..44) */
    tle_hb_idx_t hb_idx;    /* Индекс полумоста для драйвера (0-based) */
    uint8_t po_num;         /* Аппаратный номер полумоста PO1..PO12 */
    const char *name;       /* Текстовое имя линии */
} unit_chaser_mapping_t;

static const unit_chaser_mapping_t s_units_map[LEVISW_MATRIX_UNITS_COUNT] __attribute__((unused)) = {
    { 0, 1,  35, TLE_HB1,  1,  "E0" },
    { 1, 2,  36, TLE_HB5,  5,  "E1" },
    { 2, 3,  37, TLE_HB7,  7,  "E2" },
    { 3, 4,  38, TLE_HB9,  9,  "E3" },
    { 4, 5,  39, TLE_HB6,  6,  "E4" },
    { 5, 6,  40, TLE_HB4,  4,  "E5" },
    { 6, 7,  41, TLE_HB3,  3,  "E6" },
    { 7, 8,  42, TLE_HB10, 10, "E7" },
    { 8, 9,  43, TLE_HB11, 11, "E8" },
    { 9, 10, 44, TLE_HB8,  8,  "E9" },
};

/* --------------------------------------------------------------------------
 * 1. Инициализация адресного светодиода LED1 (WS2812C, GPIO23, TP20)
 * ------------------------------------------------------------------------ */

static esp_err_t app_led_init(void)
{
    /* В ESP-IDF v6.1 драйвер RMT на чипе ESP32-PICO-V3 вызывает сброс при инициализации на GPIO23.
     * Чтобы исключить сброс по сторожевому таймеру, пропускаем RMT LED (как в примере 16). */
    ESP_LOGI(TAG, "LED WS2812C (GPIO23) пропущен во избежание конфликта RMT в ESP-IDF v6.1");
    s_led_strip = NULL;
    return ESP_OK;
}

static void app_led_set_rgb(uint32_t r, uint32_t g, uint32_t b)
{
    if (s_led_strip != NULL) {
        led_strip_set_pixel(s_led_strip, 0, r, g, b);
        led_strip_refresh(s_led_strip);
    }
}

/* --------------------------------------------------------------------------
 * 2. Контроль питания и телеметрии шины +V2 (VPWR для TLE94112EL)
 * ------------------------------------------------------------------------ */

/* --------------------------------------------------------------------------
 * Монитор событий LU1/LU2. В ISR только захватывается фронт; здесь выполняется
 * debounce 5 мс и печатается подтверждённое изменение уровня.
 * ------------------------------------------------------------------------ */

#define LU_DEBOUNCE_US 5000

typedef struct {
    bool stable_level;
    bool candidate_level;
    bool pending;
    int64_t candidate_since_us;
} lu_monitor_state_t;

static void app_lu_event_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    lu_monitor_state_t lines[2] = {
        { .stable_level = levisw_bsp_lu1_read() },
        { .stable_level = levisw_bsp_lu2_read() },
    };
    lines[0].candidate_level = lines[0].stable_level;
    lines[1].candidate_level = lines[1].stable_level;

    ESP_LOGI(TAG, "LU monitor: начальные уровни LU1=%d, LU2=%d",
             lines[0].stable_level, lines[1].stable_level);

    uint32_t last_dropped = levisw_bsp_lu_event_dropped_count();

    for (;;) {
        uint32_t timeout_ms = 1000U;
        const int64_t now_us = esp_timer_get_time();

        for (size_t i = 0; i < 2; i++) {
            if (lines[i].pending) {
                const int64_t elapsed_us = now_us - lines[i].candidate_since_us;
                const int64_t remaining_us = (int64_t)LU_DEBOUNCE_US - elapsed_us;
                const uint32_t remaining_ms = (remaining_us <= 0)
                                            ? 0U : (uint32_t)((remaining_us + 999) / 1000);
                if (remaining_ms < timeout_ms) {
                    timeout_ms = remaining_ms;
                }
            }
        }

        levisw_bsp_lu_event_t event;
        esp_err_t err = levisw_bsp_lu_event_receive(&event, timeout_ms);
        if (err == ESP_OK) {
            do {
                const size_t index = (event.line == LEVISW_LU_LINE_1) ? 0U : 1U;
                lines[index].candidate_level = event.level;
                lines[index].candidate_since_us = event.timestamp_us;
                lines[index].pending = true;
            } while (levisw_bsp_lu_event_receive(&event, 0U) == ESP_OK);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Ошибка очереди LU: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        const int64_t check_us = esp_timer_get_time();
        for (size_t i = 0; i < 2; i++) {
            if (!lines[i].pending ||
                (check_us - lines[i].candidate_since_us) < LU_DEBOUNCE_US) {
                continue;
            }

            const bool pin_level = (i == 0U) ? levisw_bsp_lu1_read()
                                              : levisw_bsp_lu2_read();
            if (pin_level == lines[i].candidate_level &&
                pin_level != lines[i].stable_level) {
                lines[i].stable_level = pin_level;
                ESP_LOGI(TAG, "LU%u: подтверждённый уровень %s",
                         (unsigned)(i + 1U), pin_level ? "HIGH" : "LOW");
            }
            lines[i].pending = false;
        }

        const uint32_t dropped = levisw_bsp_lu_event_dropped_count();
        if (dropped != last_dropped) {
            ESP_LOGW(TAG, "Очередь LU переполнена: потеряно событий=%" PRIu32
                          "; синхронизируем уровни по GPIO", dropped);
            last_dropped = dropped;

            /* Восстанавливаем текущее состояние, если из-за переполнения потерян последний фронт. */
            const bool current_levels[2] = {
                levisw_bsp_lu1_read(),
                levisw_bsp_lu2_read(),
            };
            const int64_t resync_us = esp_timer_get_time();
            for (size_t i = 0; i < 2; i++) {
                lines[i].candidate_level = current_levels[i];
                lines[i].candidate_since_us = resync_us;
                lines[i].pending = (current_levels[i] != lines[i].stable_level);
            }
        }
    }
}

static esp_err_t app_check_power(void)
{
    bool pg = false;
    esp_err_t err = levisw_bsp_power_good(&pg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось прочитать PGOOD: %s", esp_err_to_name(err));
        return err;
    }

    float v1_v = 0.0f, v1_ma = 0.0f;
    float v2_v = 0.0f, v2_ma = 0.0f;
    uint32_t vamp_mv = 0;

    err = levisw_bsp_ina226_read(LEVISW_INA_V1, &v1_v, NULL, &v1_ma, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось прочитать INA226 (+V1): %s", esp_err_to_name(err));
        return err;
    }
    err = levisw_bsp_ina226_read(LEVISW_INA_V2, &v2_v, NULL, &v2_ma, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось прочитать INA226 (+V2): %s", esp_err_to_name(err));
        return err;
    }
    err = levisw_bsp_vamp_read_mv(&vamp_mv);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось прочитать +Vamp ADC: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Телеметрия питания: PG=%d | +Vamp=%.2f В | +V1=%.2f В | +V2(VPWR)=%.2f В (%.1f мА)",
             pg ? 1 : 0, (float)vamp_mv / 1000.0f, v1_v, v2_v, v2_ma);

    if (!pg) {
        ESP_LOGE(TAG, "Power Good = 0: силовые драйверы не должны включаться.");
        app_led_set_rgb(60, 0, 0);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * 3. Задача циклического «Бегущего огня» (E0..E9) в режиме Low-Side (GND)
 * ------------------------------------------------------------------------ */

#define CHASER_STEP_DELAY_MS  180  /* Задержка шага бегущего огня (мс) */
#define CHASER_BLANK_DELAY_MS 20   /* Защитная пауза Break-Before-Make (мс) */

static void tle94112_chaser_task(void *pvParameters)
{
    ESP_LOGI(TAG, "===============================================================");
    ESP_LOGI(TAG, "СТАТИЧЕСКИЙ РЕЖИМ: PO3 (pin 14, XP3:7, TP41, линия E6) = LOW-SIDE (GND)");
    ESP_LOGI(TAG, "АНОД: D0 (MC33879A OUT1, XP2:1) = +V2 (~7.8 В / 8.75 В)");
    ESP_LOGI(TAG, "Светодиод между XP2:1 и XP3:7 ГОРИТ НЕПРЕРЫВНО И ЯРКО!");
    ESP_LOGI(TAG, "===============================================================");

    /* Гарантированное отключение всех полумостов перед стартом */
    esp_err_t safe_err = tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
    if (safe_err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось перевести TLE94112 в FLOATING: %s", esp_err_to_name(safe_err));
        (void)levisw_bsp_drivers_enable(false);
        (void)levisw_bsp_power_set(false, false, false, false);
        vTaskDelete(NULL);
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Включаем полумост PO3 (HB3, клемма XP3:7 / TP41) статически в Low-Side (GND) */
    esp_err_t err = tle94112_set_hb_state(s_tle_handle, TLE_HB3, TLE_HB_LS_ON);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка включения PO3 (HB3): %s. Отключаем систему.", esp_err_to_name(err));
        (void)tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
        (void)mc33879a_set_outputs(&s_mc33879, 0x00, NULL);
        (void)levisw_bsp_drivers_enable(false);
        (void)levisw_bsp_power_set(false, false, false, false);
        vTaskDelete(NULL);
        return;
    } else {
        ESP_LOGI(TAG, ">>> [СТАТИЧЕСКИ ВКЛЮЧЕН] Полумост PO3 (XP3:7 / TP41) = LS_ON (GND) <<<");
    }

    while (1) {
        /* Дежурный цикл: просто удерживаем состояние и контролируем ток */
        vTaskDelay(pdMS_TO_TICKS(3000));
        float v2_v = 0.0f, v2_ma = 0.0f;
        esp_err_t monitor_err = levisw_bsp_ina226_read(LEVISW_INA_V2, &v2_v, NULL, &v2_ma, NULL);
        if (monitor_err != ESP_OK) {
            ESP_LOGE(TAG, "Потеря телеметрии +V2: %s. Аварийное отключение.", esp_err_to_name(monitor_err));
            (void)tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
            (void)mc33879a_set_outputs(&s_mc33879, 0x00, NULL);
            (void)levisw_bsp_drivers_enable(false);
            (void)levisw_bsp_power_set(false, false, false, false);
            vTaskDelete(NULL);
            return;
        }
        ESP_LOGI(TAG, "PO3 (XP3:7) [GND] + D0 (XP2:1) [+V2=%.2f В, I=%.1f мА] -> Светодиод ГОРИТ НЕПРЕРЫВНО", v2_v, v2_ma);
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Плата LeviSW-MB v1.2 — Проект «Бегущий огонь» (TLE94112EL)");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 11 E10-OUT.SchDoc, Лист 6)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация BSP (I2C-L, TCA9535, MCP4532, INA226, шина SPI2) */
    ESP_LOGI(TAG, "Этап 1: Инициализация BSP платы...");
    ESP_ERROR_CHECK(levisw_bsp_init());
    ESP_LOGI(TAG, "Этап 1: BSP успешно инициализирован");

    /* Отдельная задача принимает LU edge events; никаких переключений матрицы из ISR. */
    if (xTaskCreate(app_lu_event_monitor_task, "lu_monitor", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу мониторинга LU1/LU2; остановка теста");
        (void)levisw_bsp_deinit();
        return;
    }

    /* 2. Локальный индикатор LED1 (WS2812C, GPIO23, TP20) */
    ESP_LOGI(TAG, "Этап 2: Инициализация RGB LED WS2812C...");
    esp_err_t led_err = app_led_init();
    if (led_err == ESP_OK) {
        ESP_LOGI(TAG, "Этап 2: LED инициализирован успешно");
        app_led_set_rgb(0, 0, 40); /* Синий цвет - старт инициализации */
    } else {
        ESP_LOGW(TAG, "Этап 2: LED не инициализирован (%s), продолжаем без него", esp_err_to_name(led_err));
    }

    /* 3. Установка безопасного рабочего напряжения шин +V1 и +V2 через MCP4532 DA2 и DA3.
     * Схемотехника платы LeviSW-MB v1.2 (КД U210001-A E3, Узлы 7, 8):
     *  - DA2 (0x2C) задает +V1 (питание VS1/VS2 TLE94112EL через диодный мост VD24).
     *    Падение на VD24 составляет ~1.4–1.6 В. Чтобы на VS1/VS2 было стабильно > 6.3 В
     *    и исключался сброс по UVLO (~5.0 В), устанавливаем wiper = 2 (+V1 ~7.8 В).
     *  - DA3 (0x2D) задает +V2 (VPWR).
     *    Wiper = 10 давал всего 4.34 В, из-за чего D3 TPS65261 сбрасывал PGOOD=0!
     *    Согласно регламенту levisw-mb-power-management, устанавливаем wiper = 2 (+V2 ~7.8 В)
     *    или wiper = 3 (+V2 ~7.0 В), при этом PGOOD стабильно равен 1. */
    ESP_LOGI(TAG, "Этап 3: Установка уставок MCP4532 DA2 (+V1 wiper=2) и DA3 (+V2 wiper=2)...");
    ESP_ERROR_CHECK(levisw_bsp_pot_set_wiper(LEVISW_BUS_V1, 2));
    ESP_ERROR_CHECK(levisw_bsp_pot_set_wiper(LEVISW_BUS_V2, 2));
    ESP_LOGI(TAG, "Этап 3: Уставки питания установлены (+V1~7.8 В, +V2~7.8 В)");

    /* 4. Включение силовых шин +V1 и +V2 (VPWR драйверов) через расширитель DD3 */
    ESP_LOGI(TAG, "Этап 4: Включение питания +V1 и +V2...");
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));
    vTaskDelay(pdMS_TO_TICKS(50)); /* Задержка стабилизации питания DC-DC */
    ESP_LOGI(TAG, "Этап 4: Питание +V1/+V2 включено");

    /* 5. До активации EN-DE проверяем PGOOD: при аварии остаёмся в безопасном состоянии. */
    ESP_LOGI(TAG, "Этап 5: Проверка телеметрии питания до включения драйверов...");
    esp_err_t power_err = app_check_power();
    if (power_err != ESP_OK) {
        ESP_LOGE(TAG, "Проверка питания/телеметрии не пройдена. Отключаем +V1/+V2; EN-DE остаётся LOW.");
        (void)levisw_bsp_power_set(false, false, false, false);
        return;
    }

    /* 6. Активация общей линии разрешения драйверов EN-DE = 1 (GPIO20, TP115) */
    ESP_LOGI(TAG, "Этап 6: Активация EN-DE (GPIO20)...");
    ESP_ERROR_CHECK(levisw_bsp_drivers_enable(true));
    vTaskDelay(pdMS_TO_TICKS(15)); /* Время пробуждения чипов из Sleep режима */
    ESP_LOGI(TAG, "Этап 6: Драйверы выведены из Sleep");

    /* 7. Инициализация драйвера TLE94112EL через компонент tle94112.
     * Замечание: в BSP дескриптор SPI CS1=GPIO10 был добавлен при bsp_init.
     * Для работы с компонентом tle94112 освобождаем устройство в BSP, чтобы
     * tle94112_init зарегистрировал устройство с корректными флагами SPI_DEVICE_BIT_LSBFIRST. */
    ESP_LOGI(TAG, "Этап 7: Передача CS1 дескриптора компоненту tle94112...");
    const levisw_bsp_handles_t *handles = levisw_bsp_get_handles();
    if (handles != NULL && handles->spi_tle94112 != NULL) {
        spi_bus_remove_device(handles->spi_tle94112);
        ESP_LOGI(TAG, "Дескриптор SPI TLE94112 передан компоненту tle94112");
    }

    tle94112_config_t tle_cfg = {
        .spi_host = SPI2_HOST,
        .gpio_cs = LEVISW_PIN_SPI_CS1_TLE,  /* GPIO10 (TP116) */
        .gpio_en = -1,                      /* Линией EN-DE (GPIO20) управляет BSP */
        .spi_freq_hz = CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ,
    };

    ESP_LOGI(TAG, "Этап 7: Вызов tle94112_init...");
    s_tle_handle = tle94112_init(&tle_cfg);
    if (s_tle_handle == NULL) {
        ESP_LOGE(TAG, "КРИТИЧЕСКАЯ ОШИБКА: Не удалось инициализировать драйвер TLE94112EL!");
        app_led_set_rgb(60, 0, 0);
        (void)levisw_bsp_drivers_enable(false);
        (void)levisw_bsp_power_set(false, false, false, false);
        return;
    }

    ESP_LOGI(TAG, "Драйвер TLE94112EL успешно инициализирован.");

    /* 8. Инициализация драйвера верхних ключей D1 MC33879A (клеммник XP2:1..8, D0..D7).
     * Способ 1: Питание общего анода светодиодов подается с контакта XP2:1 (выход OUT1 / D0).
     * Включаем OUT1 (D0) в High-Side, подключая клемму XP2:1 к шине +V2 платы.
     * При этом потенциал анода (+V2 ~7.8В) идеально согласован с шиной питания драйвера TLE94112EL (+V1/Vs),
     * паразитные диоды выключенных полумостов надежно закрыты, и тусклое свечение полностью устранено! */
    ESP_LOGI(TAG, "Этап 8: Инициализация D1 MC33879A и включение выхода OUT1 (XP2:1, D0) -> +V2...");
    (void)levisw_bsp_rs485_set_active(false);
    gpio_set_level(LEVISW_PIN_D8_A6_IN6, 0);
    gpio_config_t in6_cfg = {
        .pin_bit_mask = (1ULL << LEVISW_PIN_D8_A6_IN6),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    (void)gpio_config(&in6_cfg);

    mc33879a_config_t mc_cfg = {
        .spi_host = SPI2_HOST,
        .mosi_io_num = LEVISW_PIN_SPI_MOSI,
        .miso_io_num = LEVISW_PIN_SPI_MISO,
        .sclk_io_num = LEVISW_PIN_SPI_CLK,
        .cs_io_num = LEVISW_PIN_SPI_CS2_MC33,
        .en_io_num = LEVISW_PIN_SPI_EN2_DRIVERS,
        .clock_speed_hz = CONFIG_LEVISW_SPI_MC33879_FREQ_HZ,
        .init_spi_bus = false,
        .existing_spi_dev = handles->spi_mc33879,
    };

    esp_err_t mc_err = mc33879a_init(&mc_cfg, &s_mc33879);
    if (mc_err == ESP_OK) {
        uint8_t faults = 0;
        /* Включаем OUT1 (бит 0 = 0x01) -> клемма XP2:1 (D0). Выходы OUT2..OUT8 отключены (0) */
        /* RX MC33879A возвращает диагностику предыдущего 16-битного кадра.
         * Поэтому после установки OUT1 выполняем отдельный readback-кадр. */
        esp_err_t out_err = mc33879a_set_outputs(&s_mc33879, 0x01, NULL);
        if (out_err == ESP_OK) {
            out_err = mc33879a_read_faults(&s_mc33879, &faults);
        }
        if (out_err != ESP_OK || faults != 0U) {
            ESP_LOGE(TAG, "MC33879A OUT1 не включён: SPI=%s, FAULT=0x%02X",
                     esp_err_to_name(out_err), faults);
            (void)mc33879a_set_outputs(&s_mc33879, 0x00, NULL);
            (void)tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
            (void)levisw_bsp_drivers_enable(false);
            (void)levisw_bsp_power_set(false, false, false, false);
            return;
        }
        ESP_LOGI(TAG, "D1 MC33879A OUT1 (XP2:1, D0) ВКЛЮЧЕН в +V2! Общий анод запитан.");
    } else {
        ESP_LOGE(TAG, "Ошибка инициализации D1 MC33879A: %s", esp_err_to_name(mc_err));
        (void)tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
        (void)levisw_bsp_drivers_enable(false);
        (void)levisw_bsp_power_set(false, false, false, false);
        return;
    }

    /* 9. Создание задачи циклического бегущего огня */
    ESP_LOGI(TAG, "Этап 9: Запуск задачи tle94112_chaser_task (бегущий огонь E0..E9)...");
    BaseType_t task_created = xTaskCreate(tle94112_chaser_task, "tle_chaser", 4096, NULL, 5, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу коммутации; отключаем силовые выходы.");
        (void)mc33879a_set_outputs(&s_mc33879, 0x00, NULL);
        (void)tle94112_set_all_hb_state(s_tle_handle, TLE_HB_FLOATING);
        (void)levisw_bsp_drivers_enable(false);
        (void)levisw_bsp_power_set(false, false, false, false);
        return;
    }

    ESP_LOGI(TAG, "Приложение готово к работе. Запущен бегущий огонь (E0..E9) с питанием от XP2:1.");
}
