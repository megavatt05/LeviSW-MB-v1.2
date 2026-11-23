/**
 * @file tle94112.h
 * @brief Драйвер микросхемы Infineon TLE94112EL для ESP-IDF
 *
 * Изменения от 2026-03-10:
 * - Добавлена структура tle94112_protection_config_t для конфигурации защит
 * - Добавлены функции: tle94112_set_protection_config(), tle94112_check_protection(),
 *   tle94112_emergency_stop(), tle94112_get_last_fault_reason()
 *
 * SPI Mode: Mode 1 (CPOL=0, CPHA=1)
 * Bit Order: LSB First (SPI_DEVICE_BIT_LSBFIRST)
 * Protocol: 16-bit frames (Address + Data)
 */

#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Индексы полумостов (HB1...HB12)
 */
typedef enum {
    TLE_HB1 = 0,  TLE_HB2,  TLE_HB3,  TLE_HB4,
    TLE_HB5,  TLE_HB6,  TLE_HB7,  TLE_HB8,
    TLE_HB9,  TLE_HB10, TLE_HB11, TLE_HB12
} tle_hb_idx_t;

/**
 * @brief Состояния полумостов
 *
 * Согласно технической документации TLE94112EL:
 * - Бит 0 (LSx): 1 = включён Low-Side (OUT → GND)
 * - Бит 1 (HSx): 1 = включён High-Side (VS → OUT)
 */
typedef enum {
    TLE_HB_FLOATING = 0b00, ///< Высокое сопротивление (выход отключён, OFF)
    TLE_HB_LS_ON    = 0b01, ///< Включён Low-Side (OUT подключён к GND)
    TLE_HB_HS_ON    = 0b10, ///< Включён High-Side (OUT подключён к VS)
    TLE_HB_BRAKE    = 0b11  ///< Включены HS и LS (короткое торможение)
} tle_hb_state_t;

/**
 * @brief Коды причин ошибок системы защиты
 */
typedef enum {
    TLE_FAULT_NONE = 0,
    TLE_FAULT_OVERCURRENT,
    TLE_FAULT_OVERVOLTAGE,
    TLE_FAULT_UNDERVOLTAGE,
    TLE_FAULT_THERMAL_SHUTDOWN,
    TLE_FAULT_THERMAL_WARNING,
    TLE_FAULT_SPI_ERROR,
    TLE_FAULT_OPEN_LOAD
} tle_fault_reason_t;

/**
 * @brief Структура конфигурации защиты
 * 
 * Включает программный контроль защит.
 * Аппаратные механизмы защиты микросхемы всегда активны.
 */
typedef struct {
    bool enable_overcurrent;      ///< Включить защиту от сверхтока
    bool enable_undervoltage;     ///< Включить защиту от пониженного напряжения (по данным INA226)
    bool enable_overvoltage;      ///< Включить защиту от повышенного напряжения (по данным INA226)
    bool enable_thermal;          ///< Включить защиту от перегрева
    bool enable_openload;         ///< Включить обнаружение обрыва нагрузки
    uint16_t overcurrent_mask;    ///< Маска полумостов для контроля сверхтока (бит N соответствует HB N+1)
    uint16_t overvoltage_mv;      ///< Порог повышенного напряжения в мВ (0 = отключено)
    uint16_t undervoltage_mv;     ///< Порог пониженного напряжения в мВ (0 = отключено)
    bool auto_stop_on_fault;      ///< Автоматически отключать все полумосты при ошибке
} tle94112_protection_config_t;

/**
 * @brief Конфигурация защиты по умолчанию
 */
#define TLE94112_PROTECTION_DEFAULTS { \
    .enable_overcurrent = true, \
    .enable_undervoltage = false, \
    .enable_overvoltage = false, \
    .enable_thermal = true, \
    .enable_openload = true, \
    .overcurrent_mask = 0xFFFF, \
    .overvoltage_mv = 0, \
    .undervoltage_mv = 0, \
    .auto_stop_on_fault = true \
}

/**
 * @brief Структура конфигурации
 */
typedef struct {
    spi_host_device_t spi_host;   ///< Хост SPI (SPI2_HOST, SPI3_HOST)
    int gpio_cs;                  ///< Вывод GPIO выбора микросхемы (CS)
    int gpio_en;                  ///< Вывод GPIO разрешения (EN); -1, если не используется
    int spi_freq_hz;              ///< Частота SPI (не более 5 МГц)
} tle94112_config_t;

/**
 * @brief Дескриптор экземпляра драйвера
 */
typedef struct tle94112_handle_s* tle94112_handle_t;

/**
 * @brief Инициализировать драйвер TLE94112EL
 * 
 * Настраивает SPI, включает микросхему, проверяет её идентификатор и очищает ошибки.
 *
 * @param config Указатель на структуру конфигурации
 * @return Дескриптор при успехе, NULL при ошибке
 */
tle94112_handle_t tle94112_init(const tle94112_config_t *config);

/**
 * @brief Деинициализировать драйвер и освободить ресурсы
 */
void tle94112_deinit(tle94112_handle_t handle);

/**
 * @brief Установить состояние одного полумоста
 */
esp_err_t tle94112_set_hb_state(tle94112_handle_t handle, tle_hb_idx_t hb, tle_hb_state_t state);

/**
 * @brief Одновременно установить состояние всех полумостов
 */
esp_err_t tle94112_set_all_hb_state(tle94112_handle_t handle, tle_hb_state_t state);

/**
 * @brief Очистить все зафиксированные флаги ошибок
 */
esp_err_t tle94112_clear_errors(tle94112_handle_t handle);

/**
 * @brief Прочитать регистр SYS_DIAG_1 и вывести ошибки в журнал
 */
esp_err_t tle94112_diagnose(tle94112_handle_t handle);

/**
 * @brief Прямое чтение регистра SYS_DIAG_1
 */
esp_err_t tle94112_get_sys_diag1(tle94112_handle_t handle, uint8_t *sys_diag_out);

/**
 * @brief Чтение всех регистров диагностики SYS_DIAG_1..SYS_DIAG_7
 */
esp_err_t tle94112_read_all_diag(tle94112_handle_t handle, uint8_t diag[7]);

/**
 * @brief Чтение регистров управления полумостами HB_ACT_1..3
 */
esp_err_t tle94112_read_all_act(tle94112_handle_t handle, uint8_t act[3]);

/**
 * @brief Получить состояние сверхтока для всех полумостов
 *
 * Читает регистры SYS_DIAG_2, SYS_DIAG_3 и SYS_DIAG_4.
 *
 * @param handle Дескриптор драйвера
 * @param oc_mask Указатель на uint16_t, в который будет записан результат
 *                Бит N = 1 означает сверхток в HB(N+1)
 *                Например: бит 0 = сверхток HB1, бит 11 = сверхток HB12
 * @return ESP_OK при успешном выполнении
 */
esp_err_t tle94112_get_overcurrent(tle94112_handle_t handle, uint16_t *oc_mask);

/**
 * @brief Получить состояние обрыва нагрузки для всех полумостов
 *
 * Читает регистры SYS_DIAG_5, SYS_DIAG_6 и SYS_DIAG_7.
 *
 * @param handle Дескриптор драйвера
 * @param ol_mask Указатель на uint16_t, в который будет записан результат
 *                Бит N = 1 означает обрыв нагрузки в HB(N+1)
 *                Например: бит 0 = обрыв нагрузки HB1, бит 11 = обрыв нагрузки HB12
 * @return ESP_OK при успешном выполнении
 */
esp_err_t tle94112_get_openload(tle94112_handle_t handle, uint16_t *ol_mask);

/**
 * @brief Очистить флаги сверхтока
 */
esp_err_t tle94112_clear_overcurrent(tle94112_handle_t handle);

/**
 * @brief Очистить флаги обрыва нагрузки
 */
esp_err_t tle94112_clear_openload(tle94112_handle_t handle);

/**
 * @brief Полный диагностический отчёт (вывод всех ошибок с номерами полумостов)
 */
esp_err_t tle94112_full_diagnose(tle94112_handle_t handle);

/**
 * @brief Установить конфигурацию защиты
 * 
 * Настраивает программный контроль защит.
 * Вызывать после tle94112_init().
 * 
 * @param handle Дескриптор драйвера
 * @param config Указатель на конфигурацию защиты
 * @return ESP_OK при успешном выполнении
 */
esp_err_t tle94112_set_protection_config(tle94112_handle_t handle, 
                                         const tle94112_protection_config_t *config);

/**
 * @brief Проверить состояние защиты и выполнить действия при обнаружении ошибки
 * 
 * Эту функцию следует вызывать периодически (например, каждые 10–100 мс)
 * для контроля аварийных условий.
 * 
 * @param handle Дескриптор драйвера
 * @param voltage_mv Текущее напряжение питания в мВ (по данным INA226; 0, если данные недоступны)
 * @return TLE_FAULT_NONE при отсутствии ошибки; в противном случае — код причины ошибки
 */
tle_fault_reason_t tle94112_check_protection(tle94112_handle_t handle, uint16_t voltage_mv);

/**
 * @brief Аварийная остановка — немедленно отключить все полумосты
 * 
 * Принудительно переводит все полумосты в состояние FLOATING и очищает флаги ошибок.
 * Использовать при критических аварийных условиях.
 * 
 * @param handle Дескриптор драйвера
 * @return ESP_OK при успешном выполнении
 */
esp_err_t tle94112_emergency_stop(tle94112_handle_t handle);

/**
 * @brief Получить последнюю причину ошибки
 * 
 * Возвращает последнюю обнаруженную причину ошибки из tle94112_check_protection().
 * 
 * @param handle Дескриптор драйвера
 * @return Код последней причины ошибки
 */
tle_fault_reason_t tle94112_get_last_fault_reason(tle94112_handle_t handle);

#ifdef __cplusplus
}
#endif