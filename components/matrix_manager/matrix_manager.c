#include "matrix_manager.h"

#include <string.h>

static matrix_manager_status_t remember_first(matrix_manager_status_t first,
                                               matrix_manager_status_t next)
{
    return (first == MATRIX_MANAGER_OK) ? next : first;
}

void matrix_manager_address_map_make_sequential_candidate(
    matrix_manager_address_map_t *map)
{
    if (map == NULL) {
        return;
    }

    memset(map, 0, sizeof(*map));
    map->version = MATRIX_MANAGER_ADDRESS_MAP_VERSION;
    map->verified = false;

    for (uint8_t apartment = 1U; apartment <= MATRIX_MANAGER_FLAT_COUNT; ++apartment) {
        const uint8_t index = (uint8_t)(apartment - 1U);
        map->apartments[index].decade =
            (uint8_t)(index / MATRIX_MANAGER_UNIT_COUNT);
        map->apartments[index].unit =
            (uint8_t)(index % MATRIX_MANAGER_UNIT_COUNT);
    }
}

bool matrix_manager_address_map_validate(const matrix_manager_address_map_t *map)
{
    if ((map == NULL) || (map->version != MATRIX_MANAGER_ADDRESS_MAP_VERSION)) {
        return false;
    }

    uint16_t used[MATRIX_MANAGER_DOZEN_COUNT] = {0U};

    for (size_t i = 0U; i < MATRIX_MANAGER_FLAT_COUNT; ++i) {
        const uint8_t decade = map->apartments[i].decade;
        const uint8_t unit = map->apartments[i].unit;

        if ((decade >= MATRIX_MANAGER_DOZEN_COUNT) ||
            (unit >= MATRIX_MANAGER_UNIT_COUNT)) {
            return false;
        }

        const uint16_t bit = (uint16_t)(1U << unit);
        if ((used[decade] & bit) != 0U) {
            return false;
        }
        used[decade] = (uint16_t)(used[decade] | bit);
    }

    for (size_t decade = 0U; decade < MATRIX_MANAGER_DOZEN_COUNT; ++decade) {
        if (used[decade] != 0x03FFU) {
            return false;
        }
    }

    return true;
}

static matrix_manager_status_t safe_open_once(matrix_manager_t *manager)
{
    matrix_manager_status_t result = MATRIX_MANAGER_OK;
    matrix_manager_status_t step;

    /* Open the analog gate first, then remove both drive sources. Try every step. */
    step = manager->hw.set_mux(manager->hw.context, 0U, false);
    result = remember_first(result, step);

    step = manager->hw.set_d_mask(manager->hw.context, 0U);
    result = remember_first(result, step);

    step = manager->hw.set_all_units(manager->hw.context, MATRIX_UNIT_FLOATING);
    result = remember_first(result, step);

    step = manager->hw.set_drivers_enabled(manager->hw.context, false);
    result = remember_first(result, step);

    manager->active_apartment = 0U;
    return result;
}

static matrix_manager_status_t latch_fault(matrix_manager_t *manager,
                                            matrix_manager_status_t reason)
{
    (void)safe_open_once(manager);
    manager->last_fault = (reason == MATRIX_MANAGER_OK)
                        ? MATRIX_MANAGER_ERR_FAULT : reason;
    manager->state = MATRIX_MANAGER_STATE_FAULT;
    return MATRIX_MANAGER_ERR_FAULT;
}

void matrix_manager_default_config(matrix_manager_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->break_before_make_us = 150U;
    config->driver_wakeup_us = 15000U;
    config->route_settle_us = 500U;
    /* Deliberately no default polarity: board integration must select HS or LS. */
    config->active_unit_drive = MATRIX_UNIT_FLOATING;
}

matrix_manager_status_t matrix_manager_init(matrix_manager_t *manager,
                                             const matrix_manager_hw_ops_t *hw,
                                             const matrix_manager_config_t *config)
{
    if ((manager == NULL) || (hw == NULL) || (config == NULL) ||
        (hw->set_mux == NULL) || (hw->set_d_mask == NULL) ||
        (hw->set_all_units == NULL) || (hw->set_unit == NULL) ||
        (hw->set_drivers_enabled == NULL) || (hw->delay_us == NULL) ||
        ((config->active_unit_drive != MATRIX_UNIT_LOW_SIDE) &&
         (config->active_unit_drive != MATRIX_UNIT_HIGH_SIDE))) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }

    memset(manager, 0, sizeof(*manager));
    manager->hw = *hw;
    manager->config = *config;
    manager->initialized = true;

    const matrix_manager_status_t status = safe_open_once(manager);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }

    manager->state = MATRIX_MANAGER_STATE_OPEN;
    manager->last_fault = MATRIX_MANAGER_OK;
    return MATRIX_MANAGER_OK;
}

matrix_manager_status_t matrix_manager_set_address_map(
    matrix_manager_t *manager,
    const matrix_manager_address_map_t *map)
{
    if ((manager == NULL) || !manager->initialized ||
        (manager->state != MATRIX_MANAGER_STATE_OPEN)) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }
    if (!matrix_manager_address_map_validate(map)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }

    manager->address_map = *map;
    manager->address_map_loaded = true;
    return MATRIX_MANAGER_OK;
}

matrix_manager_status_t matrix_manager_connect(matrix_manager_t *manager,
                                                uint8_t apartment)
{
    if ((manager == NULL) || !manager->initialized) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }
    if ((apartment == 0U) || (apartment > MATRIX_MANAGER_FLAT_COUNT)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    if (manager->state == MATRIX_MANAGER_STATE_FAULT) {
        return MATRIX_MANAGER_ERR_FAULT;
    }
    if (!manager->address_map_loaded || !manager->address_map.verified) {
        return MATRIX_MANAGER_ERR_ADDRESS_MAP_UNVERIFIED;
    }
    if ((manager->state == MATRIX_MANAGER_STATE_CONNECTED) &&
        (manager->active_apartment == apartment)) {
        return MATRIX_MANAGER_OK;
    }

    matrix_manager_status_t status = safe_open_once(manager);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }
    manager->state = MATRIX_MANAGER_STATE_OPEN;

    manager->hw.delay_us(manager->hw.context, manager->config.break_before_make_us);

    const matrix_manager_coordinate_t coordinate =
        manager->address_map.apartments[(size_t)(apartment - 1U)];
    const uint8_t decade = coordinate.decade;
    const uint8_t unit = coordinate.unit;
    const uint8_t d_mask = (uint8_t)(1U << decade);

    /* Wake the drivers while all routes remain physically open. */
    status = manager->hw.set_drivers_enabled(manager->hw.context, true);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }
    manager->hw.delay_us(manager->hw.context, manager->config.driver_wakeup_us);

    /* Configure D/E while ADG1608 is disabled; close the analog gate last. */
    status = manager->hw.set_mux(manager->hw.context, decade, false);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }

    status = manager->hw.set_all_units(manager->hw.context, MATRIX_UNIT_FLOATING);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }

    status = manager->hw.set_d_mask(manager->hw.context, d_mask);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }

    status = manager->hw.set_unit(manager->hw.context, unit,
                                  manager->config.active_unit_drive);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }

    manager->hw.delay_us(manager->hw.context, manager->config.route_settle_us);

    status = manager->hw.set_mux(manager->hw.context, decade, true);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }
    manager->hw.delay_us(manager->hw.context, manager->config.route_settle_us);

    manager->active_apartment = apartment;
    manager->state = MATRIX_MANAGER_STATE_CONNECTED;
    return MATRIX_MANAGER_OK;
}

matrix_manager_status_t matrix_manager_disconnect(matrix_manager_t *manager)
{
    if ((manager == NULL) || !manager->initialized) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }
    if (manager->state == MATRIX_MANAGER_STATE_FAULT) {
        return MATRIX_MANAGER_ERR_FAULT;
    }

    const matrix_manager_status_t status = safe_open_once(manager);
    if (status != MATRIX_MANAGER_OK) {
        return latch_fault(manager, status);
    }

    manager->state = MATRIX_MANAGER_STATE_OPEN;
    manager->last_fault = MATRIX_MANAGER_OK;
    return MATRIX_MANAGER_OK;
}

matrix_manager_status_t matrix_manager_emergency_stop(matrix_manager_t *manager)
{
    if ((manager == NULL) || !manager->initialized) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }
    const matrix_manager_status_t status = safe_open_once(manager);
    manager->last_fault = (status == MATRIX_MANAGER_OK)
                        ? MATRIX_MANAGER_ERR_FAULT : status;
    manager->state = MATRIX_MANAGER_STATE_FAULT;
    return (status == MATRIX_MANAGER_OK) ? MATRIX_MANAGER_OK : MATRIX_MANAGER_ERR_FAULT;
}

matrix_manager_status_t matrix_manager_recover(matrix_manager_t *manager)
{
    if ((manager == NULL) || !manager->initialized) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }

    const matrix_manager_status_t status = safe_open_once(manager);
    if (status != MATRIX_MANAGER_OK) {
        manager->last_fault = status;
        manager->state = MATRIX_MANAGER_STATE_FAULT;
        return MATRIX_MANAGER_ERR_FAULT;
    }

    manager->last_fault = MATRIX_MANAGER_OK;
    manager->state = MATRIX_MANAGER_STATE_OPEN;
    return MATRIX_MANAGER_OK;
}

matrix_manager_state_t matrix_manager_get_state(const matrix_manager_t *manager)
{
    return (manager == NULL) ? MATRIX_MANAGER_STATE_UNINITIALIZED : manager->state;
}

uint8_t matrix_manager_get_active_apartment(const matrix_manager_t *manager)
{
    return (manager == NULL) ? 0U : manager->active_apartment;
}

bool matrix_manager_is_ready(const matrix_manager_t *manager)
{
    return (manager != NULL) && manager->initialized &&
           ((manager->state == MATRIX_MANAGER_STATE_OPEN) ||
            (manager->state == MATRIX_MANAGER_STATE_CONNECTED));
}
