/**
 * @file main.c
 * @brief Пример 01: Входные/выходные клеммы и локальная индикация для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 01 (Лист 2 принципиальной схемы Switcher.SchDoc):
 *  1. Локальная индикация: адресный RGB-светодиод LED1 (WS2812C, GPIO23, TP20);
 *  2. Кнопка режима: SW1 MODE (GPIO27, TP21) с обработкой короткого/длинного нажатия;
 *  3. Входной клеммник XP1 (INTERCOM, 9-pin):
 *     - мониторинг линий детекции состояния трубки LU1 (GPIO36, TP7) и LU2 (GPIO37, TP6);
 *     - имитация импульса отпирания замка на линии XP1:9 (Open/1W) через перемычку R1 (0 Ом)
 *       от выхода SW-OUT2 (полумост PO2 драйвера TLE94112EL);
 *  4. Выходные клеммники матрицы:
 *     - XP2 (D8 OUT, 8-pin): поочередная коммутация десятков D0..D7 (TP27..TP34) через DA1 ADG1608;
 *     - XP3 (E10 OUT, 10-pin): контроль таблицы соответствия единиц E0..E9 (TP35..TP44)
 *       и полумостов PO1..PO12 драйвера D2 TLE94112EL.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "levisw_mb_v1_2_pins.h"
#include "levisw_mb_v1_2_bsp.h"

static const char *TAG = "EX01_TERMINALS";

/* Состояния локального индикатора LED1 */
typedef enum {
    LED_STATE_STANDBY = 0,    /* Дежурный режим: мягкое зелёное дыхание */
    LED_STATE_SERVICE,        /* Сервисный режим: синий индикатор */
    LED_STATE_CALL_LU,        /* Линия LU активна (трубка снята): оранжевый */
    LED_STATE_DOOR_OPEN,      /* Импульс открытия замка (OpenD): яркая вспышка */
    LED_STATE_FAULT           /* Ошибка питания (Power Good = 0): красный мигающий */
} app_led_state_t;

static led_strip_handle_t s_led_strip = NULL;
static volatile app_led_state_t s_current_led_state = LED_STATE_STANDBY;
static volatile bool s_service_mode_active = false;

/* --------------------------------------------------------------------------
 * 1. Управление адресным светодиодом LED1 WS2812C (GPIO23, TP20)
 * ------------------------------------------------------------------------ */

static esp_err_t app_led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LEVISW_PIN_LED_WS2812,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, /* 10 МГц */
        .flags = {
            .with_dma = false,
        }
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err == ESP_OK) {
        led_strip_clear(s_led_strip);
        ESP_LOGI(TAG, "Светодиод LED1 (WS2812C, GPIO23, TP20) успешно инициализирован");
    } else {
        ESP_LOGE(TAG, "Ошибка инициализации RMT для LED1: %s", esp_err_to_name(err));
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

/* Фоновая задача управления световыми эффектами индикатора */
static void led_indicator_task(void *pvParameters)
{
    uint32_t breath_phase = 0;
    bool blink_toggle = false;

    while (1) {
        switch (s_current_led_state) {
        case LED_STATE_STANDBY: {
            /* Плавное изменение яркости зелёного индикатора (дыхание) */
            uint32_t brightness = (breath_phase < 128) ? breath_phase : (255 - breath_phase);
            uint32_t g_val = (brightness * 40) / 255 + 5; /* Мягкое свечение 5..45 */
            app_led_set_rgb(0, g_val, 0);
            breath_phase = (breath_phase + 4) % 256;
            vTaskDelay(pdMS_TO_TICKS(25));
            break;
        }

        case LED_STATE_SERVICE:
            /* Сервисный режим: постоянное синее свечение средней яркости */
            app_led_set_rgb(0, 20, 80);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case LED_STATE_CALL_LU:
            /* Детекция трубки: янтарно-оранжевый сигнал */
            app_led_set_rgb(70, 35, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case LED_STATE_DOOR_OPEN:
            /* Импульс открытия замка: яркая бирюзовая вспышка на 400 мс */
            app_led_set_rgb(80, 255, 180);
            vTaskDelay(pdMS_TO_TICKS(400));
            s_current_led_state = s_service_mode_active ? LED_STATE_SERVICE : LED_STATE_STANDBY;
            break;

        case LED_STATE_FAULT:
            /* Авария: мигающий красный сигнал */
            blink_toggle = !blink_toggle;
            if (blink_toggle) {
                app_led_set_rgb(100, 0, 0);
            } else {
                app_led_set_rgb(0, 0, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }
    }
}

/* --------------------------------------------------------------------------
 * 2. Импульс отпирания двери на клемме XP1:9 (Open/1W) через R1 (0 Ом)
 * ------------------------------------------------------------------------ */

static void trigger_door_open_pulse(void)
{
    ESP_LOGI(TAG, ">>> [XP1:9 Open/1W] Сформирован импульс открытия замка (R1 0 Ом -> SW-OUT2 / PO2)");
    s_current_led_state = LED_STATE_DOOR_OPEN;

    /*
     * В реальной схеме линия OpenD подключена через R1 (0 Ом) к выходу PO2 TLE94112EL.
     * Здесь демонстрируется разрешающая последовательность драйверов:
     */
    levisw_bsp_drivers_enable(true);
    /* Выдача импульса через полумост PO2 (индекс LEVISW_TLE_HB_SW_OUT2 = 2) */
    vTaskDelay(pdMS_TO_TICKS(500));
    levisw_bsp_drivers_enable(false);
}

/* --------------------------------------------------------------------------
 * 3. Кнопка SW1 MODE (GPIO27, TP21, TP58, TP70)
 * ------------------------------------------------------------------------ */

/*
 * ВАЖНО: Вывод GPIO27 является аппаратно разделённым:
 *  - RS-ON -> R11 (1k) -> DD2 MAX13487 nSHDN (TP55);
 *  - BUTTON -> R10 (4.7k) <- SW1 MODE (TP21, TP58; pull-up R9 4.7k к +3.3V);
 *  - D8-A5 -> RN13 -> D1 MC33879 IN5 (TP68, pull-down RN21 1k).
 * Для опроса кнопки мы настраиваем GPIO27 как вход с подтяжкой.
 */
static void button_task(void *pvParameters)
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << LEVISW_PIN_BUTTON_MODE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    int press_ticks = 0;
    bool was_pressed = false;

    while (1) {
        /* При нажатии кнопки SW1 вывод замыкается через R10 (4.7k) на GND -> логический 0 */
        bool is_pressed = (gpio_get_level(LEVISW_PIN_BUTTON_MODE) == 0);

        if (is_pressed) {
            press_ticks++;
        } else {
            if (was_pressed) {
                if (press_ticks >= 20) { /* Удержание более 1 секунды (длинное нажатие) */
                    s_service_mode_active = !s_service_mode_active;
                    ESP_LOGW(TAG, "[SW1 MODE] Длинное нажатие: Сервисный режим %s",
                             s_service_mode_active ? "АКТИВИРОВАН" : "ДЕАКТИВИРОВАН");
                    s_current_led_state = s_service_mode_active ? LED_STATE_SERVICE : LED_STATE_STANDBY;
                } else if (press_ticks >= 1) { /* Короткое нажатие (антидребезг > 50 мс) */
                    ESP_LOGI(TAG, "[SW1 MODE] Короткое нажатие: Тест открытия двери (OpenD)");
                    trigger_door_open_pulse();
                }
            }
            press_ticks = 0;
        }

        was_pressed = is_pressed;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* --------------------------------------------------------------------------
 * 4. Мониторинг входного клеммника XP1:6/7 — линии детекции трубок LU1 / LU2
 * ------------------------------------------------------------------------ */

static void lu_monitor_task(void *pvParameters)
{
    bool last_lu1 = false;
    bool last_lu2 = false;

    ESP_LOGI(TAG, "Запущен мониторинг линий детекции трубок XP1:7 (LU1, TP7) и XP1:6 (LU2, TP6)");

    while (1) {
        bool cur_lu1 = levisw_bsp_lu1_read();
        bool cur_lu2 = levisw_bsp_lu2_read();

        if (cur_lu1 != last_lu1 || cur_lu2 != last_lu2) {
            ESP_LOGI(TAG, "[XP1 INTERCOM] Изменение состояния: LU1 (клемма 7, DO1=%d), LU2 (клемма 6, DO2=%d)",
                     cur_lu1, cur_lu2);

            if (cur_lu1 || cur_lu2) {
                /* Хотя бы на одной линии обнаружен сигнал снятой трубки */
                if (!s_service_mode_active && s_current_led_state != LED_STATE_DOOR_OPEN) {
                    s_current_led_state = LED_STATE_CALL_LU;
                }
            } else {
                /* Обе трубки уложены */
                if (!s_service_mode_active && s_current_led_state != LED_STATE_DOOR_OPEN) {
                    s_current_led_state = LED_STATE_STANDBY;
                }
            }

            last_lu1 = cur_lu1;
            last_lu2 = cur_lu2;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* --------------------------------------------------------------------------
 * 5. Тестирование выходных клеммников XP2 (D8 OUT) и XP3 (E10 OUT)
 * ------------------------------------------------------------------------ */

static void matrix_terminals_demo_task(void *pvParameters)
{
    uint8_t demo_dozen = 0;

    while (1) {
        if (s_service_mode_active) {
            /* В сервисном режиме поочерёдно переключаем клеммы десятков D0..D7 */
            ESP_LOGI(TAG, "--- ТЕСТ ВЫХОДНЫХ КЛЕММНИКОВ ---");
            ESP_LOGI(TAG, "Выбор клеммы десятков XP2:%u (D%u, TP%u) через ADG1608 (SW-EN=1)",
                     demo_dozen + 1, demo_dozen, 27 + demo_dozen);

            /* Коммутируем десяток на мультиплексоре DA1 ADG1608BR */
            levisw_bsp_adg1608_select(demo_dozen, true);

            /* Выводим в лог соответствие единиц E0..E9 на клеммнике XP3:1..10 */
            for (uint8_t unit = 0; unit < LEVISW_MATRIX_UNITS_COUNT; unit++) {
                uint8_t hb = levisw_bsp_unit_to_tle_hb(unit);
                ESP_LOGI(TAG, "  Клемма единиц XP3:%2u (E%u, TP%2u) -> полумост PO%u драйвера TLE94112EL",
                         unit + 1, unit, 35 + unit, hb);
            }

            demo_dozen = (demo_dozen + 1) % LEVISW_MATRIX_DOZENS_COUNT;
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            /* В дежурном режиме отключаем мультиплексор десятков (Hi-Z) */
            levisw_bsp_adg1608_select(0, false);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

/* --------------------------------------------------------------------------
 * 6. Периодический контроль питания и аварийных сигналов
 * ------------------------------------------------------------------------ */

static void power_monitor_task(void *pvParameters)
{
    while (1) {
        bool pg = false;
        esp_err_t err_pg = levisw_bsp_power_good(&pg);

        uint32_t vamp_mv = 0;
        esp_err_t err_vamp = levisw_bsp_vamp_read_mv(&vamp_mv);

        if (err_pg == ESP_OK && !pg) {
            ESP_LOGE(TAG, "[АВАРИЯ ПИТАНИЯ] Сигнал Power Good (D3 TPS65261, TP60) = 0!");
            s_current_led_state = LED_STATE_FAULT;
        } else if (err_vamp == ESP_OK) {
            ESP_LOGD(TAG, "Телеметрия: Power Good = 1, +Vamp = %u мВ (TP81)", (unsigned int)vamp_mv);
            if (s_current_led_state == LED_STATE_FAULT) {
                s_current_led_state = s_service_mode_active ? LED_STATE_SERVICE : LED_STATE_STANDBY;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 01: Клеммы и локальная индикация");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 01: Switcher.SchDoc Лист 2)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация аппаратного BSP платы */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 (стартовые уставки уже применены) */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, true, false));

    /* 3. Инициализация адресного светодиода LED1 (WS2812C, GPIO23) */
    ESP_ERROR_CHECK(app_led_init());

    /* 4. Создание задач FreeRTOS */
    xTaskCreate(led_indicator_task, "led_task", 3072, NULL, 5, NULL);
    xTaskCreate(button_task, "btn_task", 3072, NULL, 5, NULL);
    xTaskCreate(lu_monitor_task, "lu_task", 3072, NULL, 4, NULL);
    xTaskCreate(matrix_terminals_demo_task, "term_task", 4096, NULL, 3, NULL);
    xTaskCreate(power_monitor_task, "pwr_task", 3072, NULL, 2, NULL);

    ESP_LOGI(TAG, "Система запущена. Нажмите SW1 MODE для генерации импульса OpenD или удерживайте для сервисного режима.");
}
