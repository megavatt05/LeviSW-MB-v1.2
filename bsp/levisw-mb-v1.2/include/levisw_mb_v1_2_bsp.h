/**
 * @file levisw_mb_v1_2_bsp.h
 * @brief Публичный программный интерфейс (BSP) платы LeviSW-MB v1.2 для ESP-IDF (>= 5.0).
 *
 * Обеспечивает безопасную инициализацию и управление аппаратными узлами:
 *  - шина I2C-L (DD3 TCA9535PWR, DA2/DA3/DA4 MCP4532, DD5/DD6 INA226);
 *  - шина SPI2 (драйверы D2 TLE94112EL и D1 MC33879APEKR2, линия разрешения EN-DE);
 *  - матрица 8x10: D0..D7 через MC33879, E0..E9 через TLE94112EL;
 *  - отдельный ADG1608: выбор аналогового маршрута +LINE к AMP1..AMP8/XS3;
 *  - управление подсистемами питания (+V1, +V2, +Vamp, eLOAD), аудиоусилителем и реле;
 *  - чтение линий детекции трубок (LU1/LU2), АЦП контроля +Vamp и управление узлом RS-485.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "levisw_mb_v1_2_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Дескрипторы инициализированных шин и устройств BSP. */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t i2c_tca9535;
    i2c_master_dev_handle_t i2c_pot_v1;
    i2c_master_dev_handle_t i2c_pot_v2;
    i2c_master_dev_handle_t i2c_pot_uload;
    i2c_master_dev_handle_t i2c_ina_v1;
    i2c_master_dev_handle_t i2c_ina_v2;
    spi_device_handle_t     spi_tle94112;
    spi_device_handle_t     spi_mc33879;
    adc_oneshot_unit_handle_t adc1_handle;
} levisw_bsp_handles_t;

/** Каналы цифровых потенциометров MCP4532. */
typedef enum {
    LEVISW_BUS_V1    = 0, /**< DA2 (0x2C): регулировка +V1 */
    LEVISW_BUS_V2    = 1, /**< DA3 (0x2D): регулировка +V2 */
    LEVISW_BUS_ULOAD = 2, /**< DA4 (0x2F): регулировка Uload (eLOAD) */
} levisw_power_bus_t;

/** Каналы измерителей мощности INA226 (DD7 в канале eLOAD на плате не установлен). */
typedef enum {
    LEVISW_INA_V1 = 0,    /**< DD5 (0x40): телеметрия шины +V1 (шунт R51 = 20 мОм) */
    LEVISW_INA_V2 = 1,    /**< DD6 (0x41): телеметрия шины +V2 (шунт R62 = 20 мОм) */
} levisw_ina_channel_t;

/**
 * @brief Инициализировать все шины, периферию и безопасные начальные состояния платы LeviSW-MB v1.2.
 *
 * Последовательность инициализации:
 *  1) Удерживает EN-DE = 0 (драйверы D1/D2 отключены), RS-ON = 1 (режим ожидания);
 *  2) Инициализирует шину I2C-L и добавляет устройства DD3, DA2..DA4, DD5, DD6;
 *  3) Настраивает направления портов DD3 TCA9535 (все выходы=0, ADG1608 отключён);
 *  4) Записывает стартовые уставки MCP4532 (DA2/DA3/DA4 из Kconfig) и калибрует INA226;
 *  5) Инициализирует шину SPI2 (Mode 1) для TLE94112EL и MC33879APEKR2;
 *  6) Настраивает входы детекции LU1/LU2 (GPIO36/37) и АЦП канала VAmp (ADC1_CH6).
 */
esp_err_t levisw_bsp_init(void);

/**
 * @brief Освободить ресурсы BSP (шины I2C/SPI, АЦП, мьютекс).
 * Перед освобождением переводит выходы матрицы, силовые шины и EN-DE в безопасное состояние.
 */
esp_err_t levisw_bsp_deinit(void);

/**
 * @brief Получить указатель на внутренние дескрипторы шин и устройств BSP.
 * @return Указатель на структуру или NULL, если BSP не инициализирован.
 */
const levisw_bsp_handles_t *levisw_bsp_get_handles(void);

/* --------------------------------------------------------------------------
 * Расширитель GPIO DD3 TCA9535PWR (I2C 0x20)
 * ------------------------------------------------------------------------ */

/** Прочитать текущие логические уровни на входах Port 0 и Port 1 микросхемы DD3. */
esp_err_t levisw_bsp_exp_read_inputs(uint8_t *port0, uint8_t *port1);

/** Записать полные 8-битные значения выходных защёлок Port 0 и Port 1 микросхемы DD3. */
esp_err_t levisw_bsp_exp_write_outputs(uint8_t port0, uint8_t port1);

/**
 * @brief Атомарно обновить выбранные биты выходных портов DD3 под защитой мьютекса.
 * @param p0_mask Маска изменяемых битов Port 0.
 * @param p0_val  Новые значения для битов, указанных в p0_mask.
 * @param p1_mask Маска изменяемых битов Port 1.
 * @param p1_val  Новые значения для битов, указанных в p1_mask.
 */
esp_err_t levisw_bsp_exp_update_bits(uint8_t p0_mask, uint8_t p0_val,
                                     uint8_t p1_mask, uint8_t p1_val);

/* --------------------------------------------------------------------------
 * Питание и телеметрия (+V1, +V2, +Vamp, eLOAD, MCP4532, INA226)
 * ------------------------------------------------------------------------ */

/** Включить/отключить каналы питания через расширитель DD3 (V1EN, V2EN, VAMPON, eLOADEN). */
esp_err_t levisw_bsp_power_set(bool v1_en, bool v2_en, bool vamp_on, bool eload_en);

/** Прочитать сигнал PG (Power Good от D3 TPS65261, P0.1). */
esp_err_t levisw_bsp_power_good(bool *pg);

/** Установить энергозависимую уставку (wiper 0..128) цифрового потенциометра MCP4532. */
esp_err_t levisw_bsp_pot_set_wiper(levisw_power_bus_t bus, uint8_t wiper);

/** Прочитать текущую уставку (wiper 0..128) цифрового потенциометра MCP4532. */
esp_err_t levisw_bsp_pot_get_wiper(levisw_power_bus_t bus, uint8_t *wiper);

/** Сбросить и откалибровать выбранный INA226 под шунт 20 мОм (шаг тока 0.1 мА). */
esp_err_t levisw_bsp_ina226_init(levisw_ina_channel_t ch);

/**
 * @brief Прочитать телеметрию шины из INA226 (любой выходной указатель может быть NULL).
 * @param bus_voltage_v    Напряжение на шине, В.
 * @param shunt_voltage_mv Падение напряжения на шунте 20 мОм, мВ.
 * @param current_ma       Ток шины, мА.
 * @param power_mw         Мощность шины, мВт.
 */
esp_err_t levisw_bsp_ina226_read(levisw_ina_channel_t ch,
                                 float *bus_voltage_v,
                                 float *shunt_voltage_mv,
                                 float *current_ma,
                                 float *power_mw);

/* --------------------------------------------------------------------------
 * Матрица 8x10, SPI-драйверы и аудио/релейный интерфейс (XS2)
 * ------------------------------------------------------------------------ */

/**
 * @brief Выбрать аналоговый канал ADG1608: S1..S8 -> AMP1..AMP8 (XS3).
 * @param channel Индекс канала (0..7 соответствует S1/AMP1 .. S8/AMP8).
 * @param enable  true — замкнуть SW-EN=1; false — перевести ADG1608 в Hi-Z.
 * @note Не выбирает силовую координату D0..D7; ею управляет MC33879 по SPI.
 */
esp_err_t levisw_bsp_adg1608_select(uint8_t channel, bool enable);

/**
 * @brief Преобразовать цифру единиц (0..9 -> E0..E9) в номер полумоста TLE94112EL (1..12).
 * @return Индекс полумоста (1..12) или 0 при выходе параметра unit за пределы 0..9.
 */
uint8_t levisw_bsp_unit_to_tle_hb(uint8_t unit);

/** Управлять общей линией разрешения драйверов D1/D2 (GPIO20 EN-DE, pull-down RN19). */
esp_err_t levisw_bsp_drivers_enable(bool enable);

/** Выполнить полнодуплексный 16-битный SPI-обмен с драйвером единиц D2 TLE94112EL. */
esp_err_t levisw_bsp_spi_tle94112_xfer(uint16_t tx_frame, uint16_t *rx_frame);

/** Выполнить полнодуплексный 16-битный SPI-обмен с драйвером VOffset D1 MC33879APEKR2. */
esp_err_t levisw_bsp_spi_mc33879_xfer(uint16_t tx_frame, uint16_t *rx_frame);

/** Управлять сигналами аудиоусилителя и реле на разъёме XS2 через DD3 TCA9535. */
esp_err_t levisw_bsp_amp_set(bool amp1, bool amp2, bool amp3,
                               bool rel1, bool rel2, bool led_amp);

/** Прочитать вход ALARMIN от модуля усилителя (P1.5 на DD3 TCA9535). */
esp_err_t levisw_bsp_amp_alarm(bool *alarm_active);

/* --------------------------------------------------------------------------
 * Детекция линий трубок (LU1/LU2), АЦП VAmp и интерфейс RS-485
 * ------------------------------------------------------------------------ */

/** Прочитать логический уровень линии LU1 (GPIO36 после триггера Шмитта DD1). */
bool levisw_bsp_lu1_read(void);

/** Прочитать логический уровень линии LU2 (GPIO37 после триггера Шмитта DD1). */
bool levisw_bsp_lu2_read(void);

/** Измерить напряжение шины +Vamp в милливольтах (учтён делитель 510 кОм / 75 кОм). */
esp_err_t levisw_bsp_vamp_read_mv(uint32_t *vamp_mv);

/**
 * @brief Включить или перевести в режим отключения трансивер RS-485 DD2 MAX13487EESA+.
 *
 * ВНИМАНИЕ: вывод GPIO27 (RS-ON) аппаратно разделён с кнопкой SW1 MODE (через R10=4.7к)
 * и входом D8-A5 (IN5 драйвера D1 MC33879 через RN13). Если GPIO27 настроен как выход
 * с уровнем LOW, не нажимайте SW1 надолго и учитывайте влияние на IN5 драйвера D1.
 *
 * @param active true — высокий уровень (nSHDN=1, трансивер активен);
 *               false — низкий уровень (nSHDN=0, режим Shutdown).
 */
esp_err_t levisw_bsp_rs485_set_active(bool active);

#ifdef __cplusplus
}
#endif
