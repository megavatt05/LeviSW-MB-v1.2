#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Битовые маски выходов OUT1..OUT8 (соответствуют битам D0..D7)
 */
#define MC33879_OUT1    (1U << 0)
#define MC33879_OUT2    (1U << 1)
#define MC33879_OUT3    (1U << 2)
#define MC33879_OUT4    (1U << 3)
#define MC33879_OUT5    (1U << 4)
#define MC33879_OUT6    (1U << 5)
#define MC33879_OUT7    (1U << 6)
#define MC33879_OUT8    (1U << 7)
#define MC33879_OUT_ALL (0xFFU)

/**
 * @brief Конфигурация подключения MC33879A
 */
typedef struct {
    spi_host_device_t spi_host;             /*!< Хост SPI (например, SPI2_HOST) */
    int mosi_io_num;                        /*!< GPIO для DI (MOSI) */
    int miso_io_num;                        /*!< GPIO для DO (MISO) */
    int sclk_io_num;                        /*!< GPIO для SCLK */
    int cs_io_num;                          /*!< GPIO для CS (Chip Select) */
    int en_io_num;                          /*!< GPIO для EN (-1, если пин EN притянут к VDD аппаратно) */
    int clock_speed_hz;                     /*!< Частота SPI в Гц (по даташиту до 4 МГц, рекомендуется 1-2 МГц) */
    bool init_spi_bus;                      /*!< true — инициализировать шину SPI внутри драйвера; false — шина уже инициализирована */
    spi_device_handle_t existing_spi_dev;   /*!< Опционально: готовый дескриптор SPI из BSP (если NULL — создаётся новый на cs_io_num) */
} mc33879a_config_t;

/**
 * @brief Дескриптор устройства MC33879A
 */
typedef struct {
    spi_device_handle_t spi_dev;
    int en_io_num;
    bool owns_spi_dev;              /*!< true, если SPI-устройство добавлено внутри mc33879a_init() */
    uint8_t current_out_mask;       /*!< Текущее заданное состояние ключей (биты 0..7 -> OUT1..OUT8) */
    uint8_t last_fault_mask;        /*!< Последний считанный байт ошибок (биты 0..7 -> FAULT1..FAULT8) */
} mc33879a_handle_t;

/**
 * @brief Инициализация драйвера MC33879A
 */
esp_err_t mc33879a_init(const mc33879a_config_t *config, mc33879a_handle_t *out_handle);

/**
 * @brief Деинициализация драйвера и освобождение ресурсов
 */
esp_err_t mc33879a_deinit(mc33879a_handle_t *handle, bool free_spi_bus, spi_host_device_t spi_host);

/**
 * @brief Аппаратное включение/выключение микросхемы через пин EN
 */
esp_err_t mc33879a_set_enable(mc33879a_handle_t *handle, bool enable);

/**
 * @brief Базовый 16-битный обмен по SPI (MSB first, SPI Mode 1)
 *
 * @param handle Дескриптор устройства
 * @param tx_word 16-битное слово передачи (старший байт уходит первым)
 * @param rx_word Указатель для записи 16-битного ответа (может быть NULL)
 */
esp_err_t mc33879a_transfer_raw(mc33879a_handle_t *handle, uint16_t tx_word, uint16_t *rx_word);

/**
 * @brief Установить состояние всех 8 выходов одновременно
 *
 * @param handle Дескриптор устройства
 * @param out_mask Битовая маска выходов (1 = включен, 0 = выключен), бит 0 = OUT1 ... бит 7 = OUT8
 * @param fault_mask Указатель для возврата маски ошибок (может быть NULL). 1 в бите означает высокий уровень на стоке/обрыв при OFF или КЗ при ON.
 */
esp_err_t mc33879a_set_outputs(mc33879a_handle_t *handle, uint8_t out_mask, uint8_t *fault_mask);

/**
 * @brief Включить или выключить конкретный канал (1..8) без изменения остальных
 *
 * @param handle Дескриптор устройства
 * @param channel Номер канала от 1 до 8
 * @param state true = включить ключ, false = выключить ключ
 * @param fault_mask Указатель для возврата актуальной маски ошибок (может быть NULL)
 */
esp_err_t mc33879a_set_channel(mc33879a_handle_t *handle, uint8_t channel, bool state, uint8_t *fault_mask);

/**
 * @brief Опросить статус ошибок без изменения состояния выходов
 */
esp_err_t mc33879a_read_faults(mc33879a_handle_t *handle, uint8_t *fault_mask);

#ifdef __cplusplus
}
#endif
