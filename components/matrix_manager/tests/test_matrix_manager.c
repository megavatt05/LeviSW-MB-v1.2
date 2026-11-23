#include "matrix_manager.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef enum {
    OP_MUX = 1, OP_D_MASK, OP_ALL_UNITS, OP_UNIT, OP_DRIVERS, OP_DELAY
} operation_t;

typedef struct {
    operation_t ops[256];
    uint32_t values[256];
    size_t count;
    operation_t fail_operation;
    bool fail_once;
    bool mux_enabled;
    uint8_t mux_decade;
    uint8_t d_mask;
    matrix_unit_drive_t all_units;
    uint8_t active_unit;
    matrix_unit_drive_t active_unit_drive;
    bool drivers_enabled;
} fake_hw_t;

static void record(fake_hw_t *hw, operation_t op, uint32_t value)
{
    if (hw->count < (sizeof(hw->ops) / sizeof(hw->ops[0]))) {
        hw->ops[hw->count] = op;
        hw->values[hw->count] = value;
        hw->count++;
    }
}

static matrix_manager_status_t maybe_fail(fake_hw_t *hw, operation_t op)
{
    if (hw->fail_once && (hw->fail_operation == op)) {
        hw->fail_once = false;
        return MATRIX_MANAGER_ERR_IO;
    }
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t fake_mux(void *ctx, uint8_t decade, bool enabled)
{
    fake_hw_t *hw = ctx;
    record(hw, OP_MUX, ((uint32_t)decade << 1) | (enabled ? 1U : 0U));
    hw->mux_decade = decade;
    hw->mux_enabled = enabled;
    return maybe_fail(hw, OP_MUX);
}

static matrix_manager_status_t fake_d(void *ctx, uint8_t mask)
{
    fake_hw_t *hw = ctx;
    record(hw, OP_D_MASK, mask);
    hw->d_mask = mask;
    return maybe_fail(hw, OP_D_MASK);
}

static matrix_manager_status_t fake_all_units(void *ctx, matrix_unit_drive_t drive)
{
    fake_hw_t *hw = ctx;
    record(hw, OP_ALL_UNITS, (uint32_t)drive);
    hw->all_units = drive;
    if (drive == MATRIX_UNIT_FLOATING) {
        hw->active_unit_drive = MATRIX_UNIT_FLOATING;
    }
    return maybe_fail(hw, OP_ALL_UNITS);
}

static matrix_manager_status_t fake_unit(void *ctx, uint8_t unit, matrix_unit_drive_t drive)
{
    fake_hw_t *hw = ctx;
    record(hw, OP_UNIT, ((uint32_t)unit << 8) | (uint32_t)drive);
    hw->active_unit = unit;
    hw->active_unit_drive = drive;
    return maybe_fail(hw, OP_UNIT);
}

static matrix_manager_status_t fake_drivers(void *ctx, bool enabled)
{
    fake_hw_t *hw = ctx;
    record(hw, OP_DRIVERS, enabled ? 1U : 0U);
    hw->drivers_enabled = enabled;
    return maybe_fail(hw, OP_DRIVERS);
}

static void fake_delay(void *ctx, uint32_t delay_us)
{
    fake_hw_t *hw = ctx;
    record(hw, OP_DELAY, delay_us);
}

static void approve_test_map(matrix_manager_address_map_t *map,
                             const char *record_id)
{
    map->verified = true;
    (void)snprintf(map->pcb_revision, sizeof(map->pcb_revision), "%s", "TEST-PCB");
    (void)snprintf(map->harness_revision, sizeof(map->harness_revision), "%s", "TEST-HARNESS");
    (void)snprintf(map->commissioning_record, sizeof(map->commissioning_record),
                   "%s", record_id);
}

static int setup_unmapped(matrix_manager_t *manager, fake_hw_t *fake)
{
    matrix_manager_hw_ops_t hw = {
        .context = fake,
        .set_mux = fake_mux,
        .set_d_mask = fake_d,
        .set_all_units = fake_all_units,
        .set_unit = fake_unit,
        .set_drivers_enabled = fake_drivers,
        .delay_us = fake_delay,
    };
    matrix_manager_config_t config;
    matrix_manager_default_config(&config);
    config.active_unit_drive = MATRIX_UNIT_LOW_SIDE;
    config.break_before_make_us = 150U;
    config.driver_wakeup_us = 15000U;
    config.route_settle_us = 500U;
    memset(fake, 0, sizeof(*fake));
    return (int)matrix_manager_init(manager, &hw, &config);
}

static int install_sequential_test_map(matrix_manager_t *manager)
{
    matrix_manager_address_map_t map;
    matrix_manager_address_map_make_sequential_candidate(&map);
    approve_test_map(&map, "NATIVE-TEST-ONLY");
    return (int)matrix_manager_set_address_map(manager, &map);
}

static int setup(matrix_manager_t *manager, fake_hw_t *fake)
{
    if (setup_unmapped(manager, fake) != MATRIX_MANAGER_OK) {
        return MATRIX_MANAGER_ERR_INVALID_STATE;
    }
    return install_sequential_test_map(manager);
}

static int test_unverified_map_blocks_routing_without_hardware_writes(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup_unmapped(&manager, &fake) == MATRIX_MANAGER_OK);
    CHECK(!matrix_manager_is_ready(&manager));
    fake.count = 0U;

    CHECK(matrix_manager_connect(&manager, 47U) ==
          MATRIX_MANAGER_ERR_ADDRESS_MAP_UNVERIFIED);

    matrix_manager_address_map_t candidate;
    matrix_manager_address_map_make_sequential_candidate(&candidate);
    CHECK(matrix_manager_address_map_validate(&candidate));
    CHECK(!candidate.verified);
    CHECK(matrix_manager_set_address_map(&manager, &candidate) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_connect(&manager, 47U) ==
          MATRIX_MANAGER_ERR_ADDRESS_MAP_UNVERIFIED);

    CHECK(fake.count == 0U);
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(matrix_manager_get_active_apartment(&manager) == 0U);
    return 0;
}

static int test_default_config_is_safe_until_drive_polarity_is_selected(void)
{
    matrix_manager_t manager;
    fake_hw_t fake = {0};
    matrix_manager_hw_ops_t hw = {
        .context = &fake,
        .set_mux = fake_mux,
        .set_d_mask = fake_d,
        .set_all_units = fake_all_units,
        .set_unit = fake_unit,
        .set_drivers_enabled = fake_drivers,
        .delay_us = fake_delay,
    };
    matrix_manager_config_t config;
    matrix_manager_default_config(&config);
    CHECK(config.active_unit_drive == MATRIX_UNIT_FLOATING);
    CHECK(matrix_manager_init(&manager, &hw, &config) == MATRIX_MANAGER_OK);

    matrix_manager_address_map_t map;
    matrix_manager_address_map_make_sequential_candidate(&map);
    approve_test_map(&map, "SAFE-DEFAULT-TEST");
    CHECK(matrix_manager_set_address_map(&manager, &map) == MATRIX_MANAGER_OK);
    CHECK(!matrix_manager_is_ready(&manager));

    fake.count = 0U;
    CHECK(matrix_manager_connect(&manager, 1U) == MATRIX_MANAGER_ERR_INVALID_STATE);
    CHECK(fake.count == 0U);
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_OPEN);

    config.active_unit_drive = MATRIX_UNIT_LOW_SIDE;
    memset(&fake, 0, sizeof(fake));
    CHECK(matrix_manager_init(&manager, &hw, &config) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_set_address_map(&manager, &map) == MATRIX_MANAGER_OK);
    fake.count = 0U;

    CHECK(matrix_manager_connect(&manager, 1U) == MATRIX_MANAGER_OK);
    /* Matrix is already known OPEN. Enable the drivers before issuing SPI writes. */
    CHECK(fake.ops[0] == OP_DELAY);
    CHECK(fake.ops[1] == OP_DRIVERS && fake.values[1] == 1U);
    CHECK(fake.ops[2] == OP_DELAY);

    return 0;
}

static int test_reconnect_from_open_does_not_access_disabled_drivers(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_connect(&manager, 10U) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_disconnect(&manager) == MATRIX_MANAGER_OK);
    CHECK(!fake.drivers_enabled);
    const size_t writes_after_disconnect = fake.count;
    CHECK(matrix_manager_disconnect(&manager) == MATRIX_MANAGER_OK);
    CHECK(fake.count == writes_after_disconnect);

    fake.count = 0U;
    CHECK(matrix_manager_connect(&manager, 47U) == MATRIX_MANAGER_OK);
    CHECK(fake.ops[0] == OP_DELAY);
    CHECK(fake.ops[1] == OP_DRIVERS && fake.values[1] == 1U);
    CHECK(fake.ops[2] == OP_DELAY);
    CHECK(manager.active_apartment == 47U);
    return 0;
}

static int test_invalid_or_duplicate_map_is_rejected(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup_unmapped(&manager, &fake) == MATRIX_MANAGER_OK);
    fake.count = 0U;

    matrix_manager_address_map_t invalid;
    matrix_manager_address_map_make_sequential_candidate(&invalid);
    approve_test_map(&invalid, "BAD-MAP-TEST");
    invalid.apartments[46] = invalid.apartments[0];
    CHECK(!matrix_manager_address_map_validate(&invalid));
    CHECK(matrix_manager_set_address_map(&manager, &invalid) ==
          MATRIX_MANAGER_ERR_INVALID_ARG);
    CHECK(matrix_manager_connect(&manager, 47U) ==
          MATRIX_MANAGER_ERR_ADDRESS_MAP_UNVERIFIED);
    CHECK(fake.count == 0U);
    return 0;
}

static int test_verified_map_requires_traceability_fields(void)
{
    matrix_manager_address_map_t map;
    matrix_manager_address_map_make_sequential_candidate(&map);
    map.verified = true;
    CHECK(!matrix_manager_address_map_validate(&map));
    approve_test_map(&map, "REVIEW-123");
    CHECK(matrix_manager_address_map_validate(&map));
    return 0;
}

static int test_flat_47_maps_to_d4_e6_in_sequential_test_fixture(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_is_ready(&manager));
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(!fake.mux_enabled && fake.d_mask == 0U);
    CHECK(fake.all_units == MATRIX_UNIT_FLOATING && !fake.drivers_enabled);

    fake.count = 0U;
    CHECK(matrix_manager_connect(&manager, 47U) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_CONNECTED);
    CHECK(matrix_manager_get_active_apartment(&manager) == 47U);
    CHECK(fake.mux_enabled && fake.mux_decade == 4U);
    CHECK(fake.d_mask == (1U << 4));
    CHECK(fake.active_unit == 6U);
    CHECK(fake.active_unit_drive == MATRIX_UNIT_LOW_SIDE);
    CHECK(fake.drivers_enabled);

    CHECK(fake.ops[fake.count - 2U] == OP_MUX);
    CHECK(fake.values[fake.count - 2U] == ((4U << 1) | 1U));
    CHECK(fake.ops[fake.count - 1U] == OP_DELAY);
    CHECK(fake.values[fake.count - 1U] == 500U);
    return 0;
}

static int test_custom_map_can_represent_flat_47_d0_e6_report(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup_unmapped(&manager, &fake) == MATRIX_MANAGER_OK);

    matrix_manager_address_map_t map;
    matrix_manager_address_map_make_sequential_candidate(&map);
    const matrix_manager_coordinate_t old_flat_7 = map.apartments[6];
    map.apartments[6] = map.apartments[46];
    map.apartments[46] = old_flat_7;
    approve_test_map(&map, "CUSTOM-TEST-ONLY");

    CHECK(matrix_manager_address_map_validate(&map));
    CHECK(matrix_manager_set_address_map(&manager, &map) == MATRIX_MANAGER_OK);
    fake.count = 0U;
    CHECK(matrix_manager_connect(&manager, 47U) == MATRIX_MANAGER_OK);
    CHECK(fake.mux_enabled && fake.mux_decade == 0U);
    CHECK(fake.d_mask == 0x01U);
    CHECK(fake.active_unit == 6U);
    return 0;
}

static int test_disconnect_opens_gate_before_output_changes(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_connect(&manager, 80U) == MATRIX_MANAGER_OK);

    fake.count = 0U;
    CHECK(matrix_manager_disconnect(&manager) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(matrix_manager_get_active_apartment(&manager) == 0U);
    CHECK(!fake.mux_enabled && fake.d_mask == 0U);
    CHECK(fake.all_units == MATRIX_UNIT_FLOATING && !fake.drivers_enabled);
    CHECK(fake.ops[0] == OP_MUX);
    CHECK(fake.ops[1] == OP_D_MASK);
    CHECK(fake.ops[2] == OP_ALL_UNITS);
    CHECK(fake.ops[3] == OP_DRIVERS);
    return 0;
}

static int test_all_80_addresses_map_to_one_d_and_one_e(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);

    for (uint8_t apartment = 1U; apartment <= 80U; ++apartment) {
        const uint8_t index = (uint8_t)(apartment - 1U);
        const uint8_t decade = (uint8_t)(index / 10U);
        const uint8_t unit = (uint8_t)(index % 10U);

        CHECK(matrix_manager_connect(&manager, apartment) == MATRIX_MANAGER_OK);
        CHECK(manager.state == MATRIX_MANAGER_STATE_CONNECTED);
        CHECK(manager.active_apartment == apartment);
        CHECK(fake.mux_enabled && fake.mux_decade == decade);
        CHECK(fake.d_mask == (uint8_t)(1U << decade));
        CHECK(fake.active_unit == unit);
        CHECK(fake.active_unit_drive == MATRIX_UNIT_LOW_SIDE);
    }
    return 0;
}

static int test_reroute_enforces_break_before_make(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_connect(&manager, 10U) == MATRIX_MANAGER_OK);

    fake.count = 0U;
    CHECK(matrix_manager_connect(&manager, 47U) == MATRIX_MANAGER_OK);
    CHECK(fake.ops[0] == OP_MUX && fake.values[0] == 0U);
    CHECK(fake.ops[1] == OP_D_MASK && fake.values[1] == 0U);
    CHECK(fake.ops[2] == OP_ALL_UNITS && fake.values[2] == MATRIX_UNIT_FLOATING);
    CHECK(fake.ops[3] == OP_DRIVERS && fake.values[3] == 0U);
    CHECK(fake.ops[4] == OP_DELAY && fake.values[4] == 150U);
    CHECK(manager.active_apartment == 47U);
    CHECK(fake.mux_enabled && fake.mux_decade == 4U);
    CHECK(fake.d_mask == (1U << 4));
    CHECK(fake.active_unit == 6U);
    return 0;
}

static int test_invalid_apartment_does_not_touch_hardware(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);
    fake.count = 0U;
    CHECK(matrix_manager_connect(&manager, 0U) == MATRIX_MANAGER_ERR_INVALID_ARG);
    CHECK(matrix_manager_connect(&manager, 81U) == MATRIX_MANAGER_ERR_INVALID_ARG);
    CHECK(fake.count == 0U);
    return 0;
}

static int test_failure_latches_fault_and_attempts_safe_off(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);
    fake.count = 0U;
    fake.fail_operation = OP_UNIT;
    fake.fail_once = true;

    CHECK(matrix_manager_connect(&manager, 23U) == MATRIX_MANAGER_ERR_FAULT);
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_FAULT);
    CHECK(matrix_manager_get_active_apartment(&manager) == 0U);
    CHECK(!fake.mux_enabled && fake.d_mask == 0U);
    CHECK(fake.all_units == MATRIX_UNIT_FLOATING && !fake.drivers_enabled);
    CHECK(matrix_manager_connect(&manager, 23U) == MATRIX_MANAGER_ERR_FAULT);
    return 0;
}

static int test_fault_recovery_requires_successful_safe_off(void)
{
    matrix_manager_t manager;
    fake_hw_t fake;
    CHECK(setup(&manager, &fake) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_emergency_stop(&manager) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_FAULT);
    CHECK(matrix_manager_recover(&manager) == MATRIX_MANAGER_OK);
    CHECK(matrix_manager_get_state(&manager) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(matrix_manager_is_ready(&manager));
    return 0;
}

int main(void)
{
    CHECK(test_default_config_is_safe_until_drive_polarity_is_selected() == 0);
    CHECK(test_reconnect_from_open_does_not_access_disabled_drivers() == 0);
    CHECK(test_unverified_map_blocks_routing_without_hardware_writes() == 0);
    CHECK(test_invalid_or_duplicate_map_is_rejected() == 0);
    CHECK(test_verified_map_requires_traceability_fields() == 0);
    CHECK(test_flat_47_maps_to_d4_e6_in_sequential_test_fixture() == 0);
    CHECK(test_custom_map_can_represent_flat_47_d0_e6_report() == 0);
    CHECK(test_disconnect_opens_gate_before_output_changes() == 0);
    CHECK(test_all_80_addresses_map_to_one_d_and_one_e() == 0);
    CHECK(test_reroute_enforces_break_before_make() == 0);
    CHECK(test_invalid_apartment_does_not_touch_hardware() == 0);
    CHECK(test_failure_latches_fault_and_attempts_safe_off() == 0);
    CHECK(test_fault_recovery_requires_successful_safe_off() == 0);
    puts("All matrix manager tests passed.");
    return 0;
}
