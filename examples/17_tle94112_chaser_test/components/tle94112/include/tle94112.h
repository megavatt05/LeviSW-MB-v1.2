/**
 * @file tle94112.h
 * @brief ESP-IDF Driver for Infineon TLE94112EL Motor Driver
 *
 * Изменения от 2026-03-10:
 * - Добавлена структура tle94112_protection_config_t для конфигурации защит
 * - Добавлены функции: tle94112_set_protection_config(), tle94112_check_protection(),
 *   tle94112_emergency_stop(), tle94112_get_last_fault_reason()
 *
 * SPI Mode: Mode 1 (CPOL=0, CPHA=1)
 * Bit Order: MSB First
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
 * @brief Half-Bridge indexes (HB1...HB12)
 */
typedef enum {
    TLE_HB1 = 0,  TLE_HB2,  TLE_HB3,  TLE_HB4,
    TLE_HB5,  TLE_HB6,  TLE_HB7,  TLE_HB8,
    TLE_HB9,  TLE_HB10, TLE_HB11, TLE_HB12
} tle_hb_idx_t;

/**
 * @brief Half-Bridge states
 *
 * According to TLE94112EL Data Sheet:
 * - Bit 0 (LSx): 1 = Low-Side ON (OUT → GND)
 * - Bit 1 (HSx): 1 = High-Side ON (VS → OUT)
 */
typedef enum {
    TLE_HB_FLOATING = 0b00, ///< High impedance (OFF)
    TLE_HB_LS_ON    = 0b01, ///< Low-Side ON (Connects OUT to GND)
    TLE_HB_HS_ON    = 0b10, ///< High-Side ON (Connects OUT to VS)
    TLE_HB_BRAKE    = 0b11  ///< Both HS and LS ON (Short brake)
} tle_hb_state_t;

/**
 * @brief Fault reason codes for protection system
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
 * @brief Protection configuration structure
 * 
 * Enables software-based protection monitoring.
 * Hardware-level protections are always active in the chip.
 */
typedef struct {
    bool enable_overcurrent;      ///< Enable overcurrent protection
    bool enable_undervoltage;     ///< Enable undervoltage protection (from INA226)
    bool enable_overvoltage;      ///< Enable overvoltage protection (from INA226)
    bool enable_thermal;          ///< Enable thermal shutdown protection
    bool enable_openload;         ///< Enable open-load detection
    uint16_t overcurrent_mask;    ///< HB mask for overcurrent monitoring (bit N = HB N+1)
    uint16_t overvoltage_mv;      ///< Overvoltage threshold in mV (0 = disabled)
    uint16_t undervoltage_mv;     ///< Undervoltage threshold in mV (0 = disabled)
    bool auto_stop_on_fault;      ///< Automatically disable all HBs on fault
} tle94112_protection_config_t;

/**
 * @brief Default protection configuration
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
 * @brief Configuration structure
 */
typedef struct {
    spi_host_device_t spi_host;   ///< SPI Host (SPI2_HOST, SPI3_HOST)
    int gpio_cs;                  ///< Chip Select GPIO
    int gpio_en;                  ///< Enable GPIO (-1 if not used)
    int spi_freq_hz;              ///< SPI Frequency (max 5MHz)
} tle94112_config_t;

/**
 * @brief Handle for the driver instance
 */
typedef struct tle94112_handle_s* tle94112_handle_t;

/**
 * @brief Initialize the TLE94112EL driver
 * 
 * Performs SPI setup, powers on the device, verifies ID, and clears errors.
 *
 * @param config Pointer to configuration structure
 * @return Handle on success, NULL on failure
 */
tle94112_handle_t tle94112_init(const tle94112_config_t *config);

/**
 * @brief Deinitialize driver and free resources
 */
void tle94112_deinit(tle94112_handle_t handle);

/**
 * @brief Set state for a single Half-Bridge
 */
esp_err_t tle94112_set_hb_state(tle94112_handle_t handle, tle_hb_idx_t hb, tle_hb_state_t state);

/**
 * @brief Set state for all Half-Bridges simultaneously
 */
esp_err_t tle94112_set_all_hb_state(tle94112_handle_t handle, tle_hb_state_t state);

/**
 * @brief Clear all latched error flags
 */
esp_err_t tle94112_clear_errors(tle94112_handle_t handle);

/**
 * @brief Read SYS_DIAG_1 register and log errors
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
 * @brief Get overcurrent status for all half-bridges
 *
 * Reads SYS_DIAG_2, SYS_DIAG_3, SYS_DIAG_4 registers.
 *
 * @param handle Driver handle
 * @param oc_mask Pointer to uint16_t where result will be stored
 *                Bit N = 1 means HB(N+1) has overcurrent
 *                Example: bit 0 = HB1 OC, bit 11 = HB12 OC
 * @return ESP_OK on success
 */
esp_err_t tle94112_get_overcurrent(tle94112_handle_t handle, uint16_t *oc_mask);

/**
 * @brief Get open-load status for all half-bridges
 *
 * Reads SYS_DIAG_5, SYS_DIAG_6, SYS_DIAG_7 registers.
 *
 * @param handle Driver handle
 * @param ol_mask Pointer to uint16_t where result will be stored
 *                Bit N = 1 means HB(N+1) has open load
 *                Example: bit 0 = HB1 OL, bit 11 = HB12 OL
 * @return ESP_OK on success
 */
esp_err_t tle94112_get_openload(tle94112_handle_t handle, uint16_t *ol_mask);

/**
 * @brief Clear overcurrent flags
 */
esp_err_t tle94112_clear_overcurrent(tle94112_handle_t handle);

/**
 * @brief Clear open-load flags
 */
esp_err_t tle94112_clear_openload(tle94112_handle_t handle);

/**
 * @brief Full diagnostic report (logs all errors with HB numbers)
 */
esp_err_t tle94112_full_diagnose(tle94112_handle_t handle);

/**
 * @brief Set protection configuration
 * 
 * Configures software-based protection monitoring.
 * Call this after tle94112_init().
 * 
 * @param handle Driver handle
 * @param config Pointer to protection configuration
 * @return ESP_OK on success
 */
esp_err_t tle94112_set_protection_config(tle94112_handle_t handle, 
                                         const tle94112_protection_config_t *config);

/**
 * @brief Check protection status and take action if fault detected
 * 
 * This function should be called periodically (e.g., every 10-100ms)
 * to monitor for fault conditions.
 * 
 * @param handle Driver handle
 * @param voltage_mv Current supply voltage in mV (from INA226, 0 if not available)
 * @return TLE_FAULT_NONE if no fault, otherwise fault reason
 */
tle_fault_reason_t tle94112_check_protection(tle94112_handle_t handle, uint16_t voltage_mv);

/**
 * @brief Emergency stop - disable all half-bridges immediately
 * 
 * Forces all half-bridges to FLOATING state and clears error flags.
 * Use this in case of critical fault conditions.
 * 
 * @param handle Driver handle
 * @return ESP_OK on success
 */
esp_err_t tle94112_emergency_stop(tle94112_handle_t handle);

/**
 * @brief Get last fault reason
 * 
 * Returns the last detected fault reason from tle94112_check_protection().
 * 
 * @param handle Driver handle
 * @return Last fault reason code
 */
tle_fault_reason_t tle94112_get_last_fault_reason(tle94112_handle_t handle);

#ifdef __cplusplus
}
#endif