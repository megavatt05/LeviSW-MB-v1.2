#include "matrix_manager_adg_tle.h"

#include "esp_rom_sys.h"
#include "levisw_mb_v1_2_bsp.h"

static matrix_manager_status_t from_esp(esp_err_t err)
{
    return (err == ESP_OK) ? MATRIX_MANAGER_OK : MATRIX_MANAGER_ERR_IO;
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

static matrix_manager_status_t adapter_set_decade(void *context,
                                                  uint8_t decade,
                                                  bool enable)
{
    matrix_manager_adg_tle_t *adapter = context;
    if ((adapter == NULL) || (decade >= MATRIX_MANAGER_DOZEN_COUNT)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }

    /*
     * А0..А2 выбирают D0..D7, SW-EN физически замыкает ADG1608.
     * Адрес изменяем только при enable=false (Break-Before-Make).
     */
    const esp_err_t err = levisw_bsp_adg1608_select(decade, enable);
    if (err == ESP_OK) {
        adapter->selected_decade = decade;
        adapter->mux_enabled = enable;
    }
    return from_esp(err);
}

static matrix_manager_status_t adapter_set_d_mask(void *context, uint8_t mask)
{
    matrix_manager_adg_tle_t *adapter = context;
    if (adapter == NULL) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }

    /*
     * В этой топологии нет отдельного D-драйвера и нет MC33879 в тракте.
     * D определяется адресом ADG1608. Matrix Manager вызывает эту операцию
     * после set_decade(..., false), чтобы зафиксировать ожидаемую one-hot маску.
     */
    if (mask == 0U) {
        return adapter->mux_enabled ? MATRIX_MANAGER_ERR_INVALID_STATE
                                    : MATRIX_MANAGER_OK;
    }

    const uint8_t expected = (uint8_t)(1U << adapter->selected_decade);
    if (adapter->mux_enabled || (mask != expected)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t set_one_e(matrix_manager_adg_tle_t *adapter,
                                         uint8_t unit,
                                         matrix_unit_drive_t drive)
{
    if ((adapter == NULL) || (adapter->tle == NULL) ||
        (unit >= MATRIX_MANAGER_UNIT_COUNT) ||
        ((drive != MATRIX_UNIT_FLOATING) &&
         (drive != MATRIX_UNIT_LOW_SIDE) &&
         (drive != MATRIX_UNIT_HIGH_SIDE))) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }

    /* BSP выдаёт физический номер HB (1..12); драйвер использует индекс (0..11). */
    const uint8_t hb = levisw_bsp_unit_to_tle_hb(unit);
    if ((hb == 0U) || (hb > 12U) || (hb == 2U) || (hb == 12U)) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }

    return from_esp(tle94112_set_hb_state(adapter->tle,
                                          (tle_hb_idx_t)(hb - 1U),
                                          to_tle_state(drive)));
}

static matrix_manager_status_t adapter_set_all_e(void *context,
                                                  matrix_unit_drive_t drive)
{
    matrix_manager_adg_tle_t *adapter = context;
    if ((adapter == NULL) || (adapter->tle == NULL)) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }

    /*
     * Изменяем только десять квартирных каналов E0..E9.
     * PO2 (OpenD) и PO12 (SW-OUT12) не относятся к квартирной матрице
     * и не должны перезаписываться общим set_all_hb_state().
     */
    for (uint8_t unit = 0U; unit < MATRIX_MANAGER_UNIT_COUNT; ++unit) {
        const matrix_manager_status_t status = set_one_e(adapter, unit, drive);
        if (status != MATRIX_MANAGER_OK) {
            return status;
        }
    }
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t adapter_set_e(void *context,
                                             uint8_t unit,
                                             matrix_unit_drive_t drive)
{
    return set_one_e((matrix_manager_adg_tle_t *)context, unit, drive);
}

static matrix_manager_status_t adapter_set_driver_enable(void *context, bool enable)
{
    (void)context;
    /* GPIO20 EN-DE общий для D1/D2; MC33879 физически не используется в этом проекте. */
    return from_esp(levisw_bsp_drivers_enable(enable));
}

static void adapter_delay(void *context, uint32_t delay_us)
{
    (void)context;
    esp_rom_delay_us(delay_us);
}

esp_err_t matrix_manager_adg_tle_make_ops(
    matrix_manager_adg_tle_t *adapter,
    tle94112_handle_t tle,
    matrix_manager_hw_ops_t *out_ops)
{
    if ((adapter == NULL) || (tle == NULL) || (out_ops == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    adapter->tle = tle;
    adapter->selected_decade = 0U;
    adapter->mux_enabled = false;

    *out_ops = (matrix_manager_hw_ops_t) {
        .context = adapter,
        .set_mux = adapter_set_decade,
        .set_d_mask = adapter_set_d_mask,
        .set_all_units = adapter_set_all_e,
        .set_unit = adapter_set_e,
        .set_drivers_enabled = adapter_set_driver_enable,
        .delay_us = adapter_delay,
    };
    return ESP_OK;
}
