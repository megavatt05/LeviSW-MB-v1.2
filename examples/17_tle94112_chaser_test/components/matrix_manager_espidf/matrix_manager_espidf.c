#include "matrix_manager_espidf.h"

#include "esp_rom_sys.h"
#include "levisw_mb_v1_2_bsp.h"

static matrix_manager_status_t from_esp_err(esp_err_t err)
{
    return (err == ESP_OK) ? MATRIX_MANAGER_OK : MATRIX_MANAGER_ERR_IO;
}

static matrix_manager_status_t adapter_set_mux(void *context,
                                                uint8_t decade,
                                                bool enable)
{
    (void)context;
    if (decade >= MATRIX_MANAGER_DOZEN_COUNT) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    return from_esp_err(levisw_bsp_adg1608_select(decade, enable));
}

static matrix_manager_status_t adapter_set_d_mask(void *context, uint8_t mask)
{
    matrix_manager_espidf_t *adapter = (matrix_manager_espidf_t *)context;
    if ((adapter == NULL) || (adapter->mc == NULL)) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }

    uint8_t faults = 0U;
    esp_err_t err = mc33879a_set_outputs(adapter->mc, mask, &faults);
    if (err != ESP_OK) {
        return MATRIX_MANAGER_ERR_IO;
    }

    /*
     * MC33879A returns diagnostics for the previous SPI frame. Read once more
     * in a separate frame so a newly selected D line is not accepted blindly.
     */
    err = mc33879a_read_faults(adapter->mc, &faults);
    if ((err != ESP_OK) || (faults != 0U)) {
        return MATRIX_MANAGER_ERR_IO;
    }
    return MATRIX_MANAGER_OK;
}

static tle_hb_state_t to_tle_state(matrix_unit_drive_t drive)
{
    switch (drive) {
    case MATRIX_UNIT_FLOATING: return TLE_HB_FLOATING;
    case MATRIX_UNIT_LOW_SIDE: return TLE_HB_LS_ON;
    case MATRIX_UNIT_HIGH_SIDE: return TLE_HB_HS_ON;
    default: return TLE_HB_FLOATING;
    }
}

static matrix_manager_status_t adapter_set_all_units(void *context,
                                                       matrix_unit_drive_t drive)
{
    matrix_manager_espidf_t *adapter = (matrix_manager_espidf_t *)context;
    if ((adapter == NULL) || (adapter->tle == NULL)) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }
    if ((drive != MATRIX_UNIT_FLOATING) &&
        (drive != MATRIX_UNIT_LOW_SIDE) &&
        (drive != MATRIX_UNIT_HIGH_SIDE)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    return from_esp_err(tle94112_set_all_hb_state(adapter->tle, to_tle_state(drive)));
}

static matrix_manager_status_t adapter_set_unit(void *context,
                                                  uint8_t unit,
                                                  matrix_unit_drive_t drive)
{
    matrix_manager_espidf_t *adapter = (matrix_manager_espidf_t *)context;
    if ((adapter == NULL) || (adapter->tle == NULL) || (unit >= MATRIX_MANAGER_UNIT_COUNT)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    if ((drive != MATRIX_UNIT_LOW_SIDE) && (drive != MATRIX_UNIT_HIGH_SIDE)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }

    /* BSP mapping is non-linear: E0..E9 -> PO1,PO5,PO7,PO9,PO6,PO4,PO3,PO10,PO11,PO8. */
    const uint8_t half_bridge = levisw_bsp_unit_to_tle_hb(unit);
    if ((half_bridge == 0U) || (half_bridge > 12U) ||
        (half_bridge == 2U) || (half_bridge == 12U)) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }

    return from_esp_err(tle94112_set_hb_state(adapter->tle,
                                               (tle_hb_idx_t)(half_bridge - 1U),
                                               to_tle_state(drive)));
}

static matrix_manager_status_t adapter_set_drivers_enabled(void *context, bool enabled)
{
    (void)context;
    return from_esp_err(levisw_bsp_drivers_enable(enabled));
}

static void adapter_delay_us(void *context, uint32_t delay_us)
{
    (void)context;
    esp_rom_delay_us(delay_us);
}

esp_err_t matrix_manager_espidf_make_ops(matrix_manager_espidf_t *adapter,
                                         tle94112_handle_t tle,
                                         mc33879a_handle_t *mc,
                                         matrix_manager_hw_ops_t *out_ops)
{
    if ((adapter == NULL) || (tle == NULL) || (mc == NULL) ||
        (mc->spi_dev == NULL) || (out_ops == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    adapter->tle = tle;
    adapter->mc = mc;

    out_ops->context = adapter;
    out_ops->set_mux = adapter_set_mux;
    out_ops->set_d_mask = adapter_set_d_mask;
    out_ops->set_all_units = adapter_set_all_units;
    out_ops->set_unit = adapter_set_unit;
    out_ops->set_drivers_enabled = adapter_set_drivers_enabled;
    out_ops->delay_us = adapter_delay_us;
    return ESP_OK;
}
