/**
 * @file main.c
 * @brief Пример 03: Дифференциальный трансивер шины RS-485 для платы LeviSW-MB v1.2.
 *
 * Демонстрирует работу аппаратного Узла 03 (Лист 3 Com-485-LU-1W.SchDoc):
 *  1. Трансивер RS-485: DD2 (MAX13487EESA+) с автоматическим управлением направлением (AutoDirection);
 *  2. UART1 подключение:
 *     - U1TXD -> GPIO2 (TP52, через сборку RN8 -> DI DD2:4);
 *     - U1RXD <- GPIO4 (TP53, через сборку RN8 <- RO DD2:1);
 *  3. Управление питанием/shutdown трансивера:
 *     - Сигнал RS-ON -> GPIO27 (TP89) -> R11 (1k) -> DD2 nSHDN (TP55, pull-up RN5 10k);
 *  4. Защита линии:
 *     - Самовосстанавливающиеся предохранители FR1, FR2 (100мА/30В/15Ом) по линиям A и B;
 *     - TVS-диодная сборка FS1 (5В/16А) для защиты от импульсных перенапряжений;
 *     - Согласующий резистор R5 (120 Ом) на линии.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#include "levisw_mb_v1_2_pins.h"
#include "levisw_mb_v1_2_bsp.h"

static const char *TAG = "EX03_RS485";

#define RS485_UART_PORT      UART_NUM_1
#define RS485_BAUD_RATE      9600
#define RS485_BUF_SIZE       1024

/* --------------------------------------------------------------------------
 * Инициализация UART1 для работы с трансивером MAX13487
 * ------------------------------------------------------------------------ */

static esp_err_t init_rs485_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate  = RS485_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* Установка параметров UART1 */
    ESP_RETURN_ON_ERROR(uart_param_config(RS485_UART_PORT, &uart_config), TAG, "Ошибка конфигурирования UART1");

    /*
     * Пины UART1 для RS-485:
     *  - TX: LEVISW_PIN_RS485_TXD (GPIO2) -> RN8 -> DI (DD2:4)
     *  - RX: LEVISW_PIN_RS485_RXD (GPIO4) -> RN8 <- RO (DD2:1)
     *  - RTS / CTS не используются, так как MAX13487 имеет аппаратный AutoDirection.
     */
    ESP_RETURN_ON_ERROR(uart_set_pin(RS485_UART_PORT,
                                     LEVISW_PIN_RS485_TXD,
                                     LEVISW_PIN_RS485_RXD,
                                     UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE),
                        TAG, "Ошибка назначения пинов UART1");

    /* Установка буферов и установка режима UART */
    ESP_RETURN_ON_ERROR(uart_driver_install(RS485_UART_PORT, RS485_BUF_SIZE * 2, 0, 0, NULL, 0),
                        TAG, "Ошибка установки драйвера UART1");

    ESP_LOGI(TAG, "UART1 инициализирован (Baudrate=%d, TX=GPIO%d, RX=GPIO%d)",
             RS485_BAUD_RATE, LEVISW_PIN_RS485_TXD, LEVISW_PIN_RS485_RXD);
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Задача приёма и передачи данных по шине RS-485
 * ------------------------------------------------------------------------ */

static void rs485_comm_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *)malloc(RS485_BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Не удалось выделить память под буфер RS-485");
        vTaskDelete(NULL);
        return;
    }

    uint32_t packet_counter = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t tx_frequency = pdMS_TO_TICKS(3000); /* Передача каждые 3 секунды */

    ESP_LOGI(TAG, "Задача обмена по RS-485 запущена. Трансивер активен (nSHDN = 1 через GPIO27).");

    while (1) {
        /* 1. Периодическая передача тестового пакета по шине RS-485 */
        char tx_str[128];
        int tx_len = snprintf(tx_str, sizeof(tx_str),
                              "[LeviSW-MB v1.2] RS-485 Packet #%lu (A/B Line Active)\r\n",
                              (unsigned long)packet_counter++);

        /* MAX13487 автоматически переключает передатчик при наличии данных на DI */
        int written = uart_write_bytes(RS485_UART_PORT, tx_str, tx_len);
        if (written > 0) {
            ESP_LOGI(TAG, "[RS-485 TX] Отправлено %d байт: %s", written, tx_str);
        } else {
            ESP_LOGW(TAG, "[RS-485 TX] Ошибка отправки данных");
        }

        /* 2. Чтение входящих данных с шины (с таймаутом ожидания) */
        int len = uart_read_bytes(RS485_UART_PORT, data, RS485_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGI(TAG, "[RS-485 RX] Получено %d байт: %s", len, (char *)data);
        }

        /* Ожидание до следующего цикла передачи (3 секунды) */
        vTaskDelayUntil(&last_wake_time, tx_frequency);
    }

    free(data);
}

/* --------------------------------------------------------------------------
 * Главная точка входа приложения
 * ------------------------------------------------------------------------ */

void app_main(void)
{
    ESP_LOGI(TAG, "===========================================================");
    ESP_LOGI(TAG, "Платы LeviSW-MB v1.2 — Пример 03: Трансивер RS-485 (MAX13487)");
    ESP_LOGI(TAG, "Схема: U210001-A E3 (Узел 03: Com-485-LU-1W.SchDoc Лист 3)");
    ESP_LOGI(TAG, "===========================================================");

    /* 1. Инициализация базового BSP платы (I2C, силовые шины, TCA9535) */
    ESP_ERROR_CHECK(levisw_bsp_init());

    /* 2. Включаем силовые шины питания +V1 и +V2 */
    ESP_ERROR_CHECK(levisw_bsp_power_set(true, true, false, false));

    /*
     * 3. Управление трансивером RS-485 через вывод GPIO27 (RS-ON):
     *    Установка уровня HIGH (1) активирует трансивер DD2 (nSHDN = 1).
     *    (Примечание: учитывайте совместное использование GPIO27 с кнопкой SW1 и входом D8-A5).
     */
    ESP_ERROR_CHECK(levisw_bsp_rs485_set_active(true));
    ESP_LOGI(TAG, "Трансивер DD2 MAX13487 переведён в активный режим (GPIO27 RS-ON = 1, TP89/TP55)");

    /* 4. Инициализация UART1 для работы с MAX13487 */
    ESP_ERROR_CHECK(init_rs485_uart());

    /* 5. Создание задачи обмена по шине RS-485 */
    xTaskCreate(rs485_comm_task, "rs485_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Пример 03 успешно запущен. Узел RS-485 функционирует.");
}
