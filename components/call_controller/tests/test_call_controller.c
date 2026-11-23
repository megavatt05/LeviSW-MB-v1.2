#include "call_controller.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct {
    unsigned matrix_writes;
    bool mux_enabled;
    uint8_t d_mask;
    matrix_unit_drive_t e_state;
    bool drivers_enabled;
} fake_hw_t;

static matrix_manager_status_t fake_mux(void *ctx, uint8_t decade, bool enable)
{
    fake_hw_t *hw = ctx;
    (void)decade;
    hw->matrix_writes++;
    hw->mux_enabled = enable;
    return MATRIX_MANAGER_OK;
}
static matrix_manager_status_t fake_d(void *ctx, uint8_t mask)
{
    fake_hw_t *hw = ctx;
    hw->matrix_writes++;
    hw->d_mask = mask;
    return MATRIX_MANAGER_OK;
}
static matrix_manager_status_t fake_all(void *ctx, matrix_unit_drive_t drive)
{
    fake_hw_t *hw = ctx;
    hw->matrix_writes++;
    hw->e_state = drive;
    return MATRIX_MANAGER_OK;
}
static matrix_manager_status_t fake_unit(void *ctx, uint8_t unit, matrix_unit_drive_t drive)
{
    fake_hw_t *hw = ctx;
    (void)unit;
    hw->matrix_writes++;
    hw->e_state = drive;
    return MATRIX_MANAGER_OK;
}
static matrix_manager_status_t fake_drivers(void *ctx, bool enabled)
{
    fake_hw_t *hw = ctx;
    hw->matrix_writes++;
    hw->drivers_enabled = enabled;
    return MATRIX_MANAGER_OK;
}
static void fake_delay(void *ctx, uint32_t us)
{
    (void)ctx;
    (void)us;
}
static int make_controller(call_controller_t *call, matrix_manager_t *matrix, fake_hw_t *fake,
                           call_controller_config_t *config)
{
    matrix_manager_hw_ops_t hw = {
        .context = fake, .set_mux = fake_mux, .set_d_mask = fake_d,
        .set_all_units = fake_all, .set_unit = fake_unit,
        .set_drivers_enabled = fake_drivers, .delay_us = fake_delay,
    };
    matrix_manager_config_t matrix_cfg;
    matrix_manager_default_config(&matrix_cfg);
    matrix_cfg.active_unit_drive = MATRIX_UNIT_LOW_SIDE;
    if (matrix_manager_init(matrix, &hw, &matrix_cfg) != MATRIX_MANAGER_OK) {
        return 1;
    }

    call_controller_default_config(config);
    config->number_capture_timeout_ms = 5000U;
    config->ring_timeout_ms = 60000U;
    config->answer_verify_timeout_ms = 300U;
    return (int)call_controller_init(call, matrix, config, NULL, NULL);
}

static call_controller_status_t send_event(call_controller_t *call,
                                             call_event_type_t type,
                                             uint8_t apartment,
                                             bool valid,
                                             uint64_t now_ms)
{
    const call_event_t event = {
        .type = type,
        .apartment = apartment,
        .decoder_valid = valid,
    };
    return call_controller_handle_event(call, &event, now_ms);
}

static int test_full_call_lifecycle_holds_route_until_hangup(void)
{
    call_controller_t call;
    matrix_manager_t matrix;
    fake_hw_t fake;
    call_controller_config_t config;
    memset(&fake, 0, sizeof(fake));
    CHECK(make_controller(&call, &matrix, &fake, &config) == CALL_CONTROLLER_OK);

    CHECK(send_event(&call, CALL_EVENT_CALL_DETECTED, 0U, false, 10U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_CAPTURE_NUMBER);
    CHECK(send_event(&call, CALL_EVENT_NUMBER_READY, 47U, true, 100U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_RINGING);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 47U);
    CHECK(fake.mux_enabled && fake.d_mask == (1U << 4));
    CHECK(send_event(&call, CALL_EVENT_PICKUP_CANDIDATE, 0U, false, 500U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_ANSWER_VERIFY);
    CHECK(send_event(&call, CALL_EVENT_PICKUP_CONFIRMED, 0U, false, 650U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_TALKING);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 47U);

    const unsigned writes_during_talk = fake.matrix_writes;
    CHECK(send_event(&call, CALL_EVENT_CALL_DETECTED, 0U, false, 700U) == CALL_CONTROLLER_ERR_BUSY);
    CHECK(fake.matrix_writes == writes_during_talk);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 47U);

    CHECK(send_event(&call, CALL_EVENT_HANGUP_DETECTED, 0U, false, 5000U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 0U);
    CHECK(!fake.mux_enabled && fake.d_mask == 0U);
    CHECK(fake.e_state == MATRIX_UNIT_FLOATING && !fake.drivers_enabled);
    return 0;
}

static int test_invalid_decoded_number_never_connects(void)
{
    call_controller_t call;
    matrix_manager_t matrix;
    fake_hw_t fake;
    call_controller_config_t config;
    memset(&fake, 0, sizeof(fake));
    CHECK(make_controller(&call, &matrix, &fake, &config) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_CALL_DETECTED, 0U, false, 1U) == CALL_CONTROLLER_OK);

    const unsigned writes_before = fake.matrix_writes;
    CHECK(send_event(&call, CALL_EVENT_NUMBER_READY, 81U, true, 100U) ==
          CALL_CONTROLLER_ERR_INVALID_NUMBER);
    CHECK(call_controller_get_state(&call) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(fake.matrix_writes == writes_before);
    return 0;
}

static int test_capture_timeout_returns_to_standby(void)
{
    call_controller_t call;
    matrix_manager_t matrix;
    fake_hw_t fake;
    call_controller_config_t config;
    memset(&fake, 0, sizeof(fake));
    CHECK(make_controller(&call, &matrix, &fake, &config) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_CALL_DETECTED, 0U, false, 100U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_tick(&call, 5099U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_CAPTURE_NUMBER);
    CHECK(call_controller_tick(&call, 5100U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    return 0;
}

static int test_answer_not_confirmed_returns_to_ringing(void)
{
    call_controller_t call;
    matrix_manager_t matrix;
    fake_hw_t fake;
    call_controller_config_t config;
    memset(&fake, 0, sizeof(fake));
    CHECK(make_controller(&call, &matrix, &fake, &config) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_CALL_DETECTED, 0U, false, 0U) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_NUMBER_READY, 5U, true, 10U) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_PICKUP_CANDIDATE, 0U, false, 20U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_tick(&call, 320U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_RINGING);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 5U);
    return 0;
}


static int test_ring_timeout_is_not_extended_by_pickup_rejections(void)
{
    call_controller_t call;
    matrix_manager_t matrix;
    fake_hw_t fake;
    call_controller_config_t config;
    memset(&fake, 0, sizeof(fake));
    CHECK(make_controller(&call, &matrix, &fake, &config) == CALL_CONTROLLER_OK);

    CHECK(send_event(&call, CALL_EVENT_CALL_DETECTED, 0U, false, 0U) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_NUMBER_READY, 6U, true, 10U) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_PICKUP_CANDIDATE, 0U, false, 20000U) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_PICKUP_REJECTED, 0U, false, 20100U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_RINGING);

    /* Ring timeout is measured from first entry to RINGING, not last pickup retry. */
    CHECK(call_controller_tick(&call, 60010U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    return 0;
}

static int test_hardware_fault_latches_until_explicit_reset(void)
{
    call_controller_t call;
    matrix_manager_t matrix;
    fake_hw_t fake;
    call_controller_config_t config;
    memset(&fake, 0, sizeof(fake));
    CHECK(make_controller(&call, &matrix, &fake, &config) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_CALL_DETECTED, 0U, false, 0U) == CALL_CONTROLLER_OK);
    CHECK(send_event(&call, CALL_EVENT_NUMBER_READY, 80U, true, 1U) == CALL_CONTROLLER_OK);

    CHECK(send_event(&call, CALL_EVENT_HARDWARE_FAULT, 0U, false, 2U) == CALL_CONTROLLER_ERR_FAULT);
    CHECK(call_controller_get_state(&call) == CALL_STATE_FAULT);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_FAULT);
    CHECK(send_event(&call, CALL_EVENT_FAULT_RESET, 0U, false, 3U) == CALL_CONTROLLER_OK);
    CHECK(call_controller_get_state(&call) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    return 0;
}

int main(void)
{
    CHECK(test_full_call_lifecycle_holds_route_until_hangup() == 0);
    CHECK(test_invalid_decoded_number_never_connects() == 0);
    CHECK(test_capture_timeout_returns_to_standby() == 0);
    CHECK(test_answer_not_confirmed_returns_to_ringing() == 0);
    CHECK(test_ring_timeout_is_not_extended_by_pickup_rejections() == 0);
    CHECK(test_hardware_fault_latches_until_explicit_reset() == 0);
    puts("All call controller tests passed.");
    return 0;
}
