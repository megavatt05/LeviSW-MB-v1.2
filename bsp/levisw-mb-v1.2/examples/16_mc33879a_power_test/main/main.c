/**
 * @file main.c
 * @brief Тестовый проект для платы LeviSW-MB v1.2: включение питания (+V1/+V2)
 *        и комплексное тестирование драйвера D1 (NXP MC33879APEKR2) через компонент mc33879a.
 *
 * Последовательность запуска на плате LeviSW-MB v1.2 (U210001-A E3):
 *  1. Инициализация BSP (шины I2C-L, расширитель DD3 TCA9535, потенциометры DA2..DA4,
 *     мониторы INA226 DD5/DD6, шина SPI2 Mode 1).
 *  2. Включение силового питания +V1 и +V2 (включая VPWR микросхемы D1 MC33879A)
 *     через DD3 TCA9535, проверка сигнала Power Good (PG, P0.1) и телеметрии INA226.
 *  3. CORN13/RN13 не установлена; IN5/IN6 не подключены к GPIO27/GPIO21.
 *     Пользователь подтвердил LOW на обоих входах.
 *  4. Инициализация драйвера mc33879a с подачей EN-DE = 1 (GPIO20, TP115) и
 *     проведение циклического теста всех 8 каналов OUT1..OUT8 с чтением маски ошибок.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "led_strip.h"

#include "levisw_mb_v1_2_pins.h"
#include "levisw_mb_v1_2_bsp.h"
#include "mc33879a.h"

static const char *TAG = "MC33879A_TEST";

static led_strip_handle_t s_led_strip = NULL;
static mc33879a_handle_t s_mc33879;

/* --------------------------------------------------------------------------
 * Вспомогательные функции: светодиод LED1 (WS2812C, GPIO23) и формат байта
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

static void format_byte_bin(uint8_t val, char out[9])
{
    for (int i = 0; i < 8; i++) {
        out[i] = (val & (1U << (7 - i))) ? '1' : '0';
    }
    out[8] = '\0';
}

/* --------------------------------------------------------------------------
 * Включение и контроль силового питания платы (+V1, +V2 / VPWR)
 * ------------------------------------------------------------------------ */

static esp_err_t board_power_enable_and_check(void)
{
    ESP_LOGI(TAG, "Включение силовых шин питания +V1 и +V2 (питание VPWR для D1 MC33879A)...");
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /* Задержка на плавный пуск D3 TPS65261 и заряд выходных конденсаторов */
    vTaskDelay(pdMS_TO_TICKS(50));

    bool pg = false;
    ESP_ERROR_CHECK(levisw_bsp_power_good(&pg));

    float v1_v = 0.0f, v1_ma = 0.0f;
    float v2_v = 0.0f, v2_ma = 0.0f;
    (void)levisw_bsp_ina226_read(LEVISW_INA_V1, &v1_v, NULL, &v1_ma, NULL);
    (void)levisw_bsp_ina226_read(LEVISW_INA_V2, &v2_v, NULL, &v2_ma, NULL);

    ESP_LOGI(TAG, "Статус питания: PG=%d | +V1=%.2f В (%.1f мА) | +V2(VPWR)=%.2f В (%.1f мА)",
             pg ? 1 : 0, v1_v, v1_ma, v2_v, v2_ma);

    if (!pg) {
        ESP_LOGW(TAG, "ВНИМАНИЕ: сигнал Power Good (PG) = 0! Проверьте входное питание POWERIN.");
        app_led_set_rgb(60, 0, 0);
    }
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Аппаратные входы IN5/IN6 на осмотренном экземпляре
 * --------------------------------------------------------------------------
 * CORN13/RN13 не установлена; входы не соединены с GPIO27/GPIO21.
 * Пользователь подтвердил LOW непосредственно на IN5 и IN6.
 */
static void prepare_mc33879_parallel_inputs(void)
{
    ESP_LOGI(TAG, "MC33879 IN5/IN6: оба LOW подтверждены; CORN13/RN13 DNP, GPIO не задействуются");
}

/* --------------------------------------------------------------------------
 * Задача циклического тестирования MC33879A
 * ------------------------------------------------------------------------ */

static void mc33879_test_task(void *pvParameters)
{
    uint8_t faults = 0;
    char out_bin[9];
    char flt_bin[9];
    uint32_t cycle = 0;

    ESP_LOGI(TAG, "Старт цикла тестирования D1 MC33879A (SPI2 Mode 1, CS2=IO8, EN-DE=IO20)");

    while (1) {
        ESP_LOGI(TAG, "================ Цикл тестирования #%lu ================", (unsigned long)cycle);

        /* 1. Включаем каналы OUT1 и OUT3 одновременно */
        uint8_t mask = MC33879_OUT1 | MC33879_OUT3;
        ESP_ERROR_CHECK(mc33879a_set_outputs(&s_mc33879, mask, &faults));
        /* У MC33879A диагностика обновляется со сдвигом на 1 кадр: делаем повторный опрос */
        vTaskDelay(pdMS_TO_TICKS(5));
        ESP_ERROR_CHECK(mc33879a_read_faults(&s_mc33879, &faults));
        format_byte_bin(mask, out_bin);
        format_byte_bin(faults, flt_bin);
        ESP_LOGI(TAG, "[OUT1 + OUT3 ON] OUT=0x%02X (%s) | FAULT/RX=0x%02X (%s)",
                 mask, out_bin, faults, flt_bin);
        app_led_set_rgb(0, 45, 0);
        vTaskDelay(pdMS_TO_TICKS(1500));

        /* 2. Дополнительно включаем канал OUT8 */
        ESP_ERROR_CHECK(mc33879a_set_channel(&s_mc33879, 8, true, &faults));
        vTaskDelay(pdMS_TO_TICKS(5));
        ESP_ERROR_CHECK(mc33879a_read_faults(&s_mc33879, &faults));
        format_byte_bin(s_mc33879.current_out_mask, out_bin);
        format_byte_bin(faults, flt_bin);
        ESP_LOGI(TAG, "[+OUT8 ON]       OUT=0x%02X (%s) | FAULT/RX=0x%02X (%s)",
                 s_mc33879.current_out_mask, out_bin, faults, flt_bin);
        vTaskDelay(pdMS_TO_TICKS(1500));

        /* 3. Поочерёдный тест каждого выхода OUT1..OUT8 (бегущий бит) */
        ESP_LOGI(TAG, "Запуск поочерёдного переключения каналов OUT1..OUT8...");
        for (uint8_t ch = 1; ch <= 8; ch++) {
            uint8_t single_mask = (uint8_t)(1U << (ch - 1));
            ESP_ERROR_CHECK(mc33879a_set_outputs(&s_mc33879, single_mask, &faults));
            vTaskDelay(pdMS_TO_TICKS(5));
            ESP_ERROR_CHECK(mc33879a_read_faults(&s_mc33879, &faults));

            format_byte_bin(single_mask, out_bin);
            format_byte_bin(faults, flt_bin);
            ESP_LOGI(TAG, "  Канал OUT%u: OUT=0x%02X (%s) -> RX=0x%02X (%s)",
                     ch, single_mask, out_bin, faults, flt_bin);
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        /* 4. Выключаем все выходы и проверяем питание */
        ESP_ERROR_CHECK(mc33879a_set_outputs(&s_mc33879, 0x00, &faults));
        vTaskDelay(pdMS_TO_TICKS(5));
        ESP_ERROR_CHECK(mc33879a_read_faults(&s_mc33879, &faults));
        format_byte_bin(faults, flt_bin);
        ESP_LOGI(TAG, "[ALL OFF]        OUT=0x00 (00000000) | FAULT/RX=0x%02X (%s)",
                 faults, flt_bin);

        bool pg = false;
        float v1_v = 0.0f, v1_ma = 0.0f, v2_v = 0.0f, v2_ma = 0.0f;
        (void)levisw_bsp_power_good(&pg);
        (void)levisw_bsp_ina226_read(LEVISW_INA_V1, &v1_v, NULL, &v1_ma, NULL);
        (void)levisw_bsp_ina226_read(LEVISW_INA_V2, &v2_v, NULL, &v2_ma, NULL);
        ESP_LOGI(TAG, "[Телеметрия]     PG=%d | +V1=%.2f В (%.1f мА) | +V2=%.2f В (%.1f мА)",
                 pg ? 1 : 0, v1_v, v1_ma, v2_v, v2_ma);

        app_led_set_rgb(pg ? 0 : 60, 0, pg ? 40 : 0); /* Синий при норме, красный при отсутствии PG */
        cycle++;
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "===============================================================");
    ESP_LOGI(TAG, "LeviSW-MB v1.2 — Проект тестирования питания и D1 MC33879A");
    ESP_LOGI(TAG, "===============================================================");

    /* 1. Инициализация светодиода и аппаратного BSP */
    ESP_ERROR_CHECK(app_led_init());
    app_led_set_rgb(40, 25, 0); /* Жёлтый: инициализация */

    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем питание (+V1, +V2/VPWR) и контролируем PG и INA226 */
    ESP_ERROR_CHECK(board_power_enable_and_check());

    /* 3. Фиксируем подтверждённое состояние IN5/IN6; GPIO-соединений нет. */
    prepare_mc33879_parallel_inputs();

    /* 4. Подключаем и включаем микросхему D1 MC33879A (EN-DE=GPIO20 -> 1) */
    const levisw_bsp_handles_t *handles = levisw_bsp_get_handles();
    ESP_ERROR_CHECK(handles != NULL ? ESP_OK : ESP_FAIL);

    mc33879a_config_t cfg = {
        .spi_host = SPI2_HOST,
        .mosi_io_num = LEVISW_PIN_SPI_MOSI,       /* GPIO7  (TP113) */
        .miso_io_num = LEVISW_PIN_SPI_MISO,       /* GPIO9  (TP111) */
        .sclk_io_num = LEVISW_PIN_SPI_CLK,        /* GPIO5  (TP118) */
        .cs_io_num = LEVISW_PIN_SPI_CS2_MC33,     /* GPIO8  (CS2 / CS-D, TP114) */
        .en_io_num = LEVISW_PIN_SPI_EN2_DRIVERS,  /* GPIO20 (EN-DE, TP115) */
        .clock_speed_hz = CONFIG_LEVISW_SPI_MC33879_FREQ_HZ,
        .init_spi_bus = false,                    /* Шина SPI2 уже инициализирована BSP */
        .existing_spi_dev = handles->spi_mc33879, /* Используем готовый дескриптор CS2=GPIO8 */
    };

    ESP_ERROR_CHECK(mc33879a_init(&cfg, &s_mc33879));

    /* 5. Запуск задачи тестирования */
    xTaskCreate(mc33879_test_task, "mc33879_test", 4096, NULL, 5, NULL);
}
