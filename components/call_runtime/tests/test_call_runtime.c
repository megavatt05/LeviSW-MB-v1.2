#include "call_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct {
    bool mux_enabled;
    uint8_t mux_decade;
    uint8_t d_mask;
    uint8_t unit;
    matrix_unit_drive_t e_state;
    bool drivers_enabled;
    unsigned writes;
    bool fail_next;
} fake_hw_t;

static matrix_manager_status_t maybe_fail(fake_hw_t *hw)
{
    if (hw->fail_next) {
        hw->fail_next = false;
        return MATRIX_MANAGER_ERR_IO;
    }
    return MATRIX_MANAGER_OK;
}
static matrix_manager_status_t fake_mux(void *ctx, uint8_t decade, bool enable)
{
    fake_hw_t *hw = ctx;
    hw->writes++;
    hw->mux_decade = decade;
    hw->mux_enabled = enable;
    return maybe_fail(hw);
}
static matrix_manager_status_t fake_d(void *ctx, uint8_t mask)
{
    fake_hw_t *hw = ctx;
    hw->writes++;
    hw->d_mask = mask;
    return maybe_fail(hw);
}
static matrix_manager_status_t fake_all(void *ctx, matrix_unit_drive_t drive)
{
    fake_hw_t *hw = ctx;
    hw->writes++;
    hw->e_state = drive;
    return maybe_fail(hw);
}
static matrix_manager_status_t fake_unit(void *ctx, uint8_t unit, matrix_unit_drive_t drive)
{
    fake_hw_t *hw = ctx;
    hw->writes++;
    hw->unit = unit;
    hw->e_state = drive;
    return maybe_fail(hw);
}
static matrix_manager_status_t fake_enable(void *ctx, bool enable)
{
    fake_hw_t *hw = ctx;
    hw->writes++;
    hw->drivers_enabled = enable;
    return maybe_fail(hw);
}
static void fake_delay(void *ctx, uint32_t us)
{
    (void)ctx;
    (void)us;
}
static int setup(call_runtime_t *runtime, matrix_manager_t *matrix, fake_hw_t *fake)
{
    matrix_manager_hw_ops_t ops = {
        .context = fake,
        .set_mux = fake_mux,
        .set_d_mask = fake_d,
        .set_all_units = fake_all,
        .set_unit = fake_unit,
        .set_drivers_enabled = fake_enable,
        .delay_us = fake_delay,
    };
    matrix_manager_config_t matrix_config;
    matrix_manager_default_config(&matrix_config);
    matrix_config.active_unit_drive = MATRIX_UNIT_LOW_SIDE;
    if (matrix_manager_init(matrix, &ops, &matrix_config) != MATRIX_MANAGER_OK) {
        return 1;
    }

    call_runtime_config_t config;
    call_runtime_default_config(&config);
    config.decoder.glitch_filter_us = 120U;
    config.decoder.burst_gap_us = 80000U;
    config.handset.off_hook_level = true;
    config.handset.pickup_confirm_ms = 60U;
    config.handset.hangup_confirm_ms = 60U;
    config.controller.number_capture_timeout_ms = 5000U;
    config.controller.ring_timeout_ms = 60000U;
    config.controller.answer_verify_timeout_ms = 300U;
    const bool initial_levels[2] = { false, false };
    return (int)call_runtime_init(runtime, matrix, &config, initial_levels,
                                  0, NULL, NULL);
}
static void emit_address(call_runtime_t *runtime,
                         uint8_t apartment,
                         int64_t *timestamp_us,
                         bool *level,
                         uint8_t line)
{
    const uint16_t count = (uint16_t)(apartment * 2U);
    for (uint16_t i = 0; i < count; ++i) {
        *level = !*level;
        *timestamp_us += 1000;
        const lu_decoder_edge_t edge = {
            .line = (lu_decoder_line_t)line,
            .level = *level,
            .timestamp_us = *timestamp_us,
        };
        (void)call_runtime_feed_lu_edge(runtime, &edge);
        (void)call_runtime_tick(runtime, *timestamp_us + 200);
    }
}
static int test_full_flow_decoder_to_talking_to_hangup(void)
{
    call_runtime_t runtime;
    matrix_manager_t matrix;
    fake_hw_t fake;
    memset(&fake, 0, sizeof(fake));
    CHECK(setup(&runtime, &matrix, &fake) == CALL_RUNTIME_OK);

    bool level = false;
    int64_t now_us = 1000;
    emit_address(&runtime, 47U, &now_us, &level, 0U);
    CHECK(call_runtime_tick(&runtime, now_us + 200U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_STANDBY);
    CHECK(call_runtime_tick(&runtime, now_us + 80200U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_RINGING);
    CHECK(call_runtime_get_apartment(&runtime) == 47U);
    CHECK(call_runtime_get_active_line(&runtime) == 0);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 47U);

    now_us += 90000U;
    level = true;
    lu_decoder_edge_t edge = { LU_DECODER_LINE_1, true, now_us };
    CHECK(call_runtime_feed_lu_edge(&runtime, &edge) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_ANSWER_VERIFY);
    CHECK(call_runtime_tick(&runtime, now_us + 59000U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_ANSWER_VERIFY);
    CHECK(call_runtime_tick(&runtime, now_us + 60000U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_TALKING);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 47U);

    now_us += 70000U;
    level = false;
    edge.level = false;
    edge.timestamp_us = now_us;
    CHECK(call_runtime_feed_lu_edge(&runtime, &edge) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_TALKING);
    CHECK(call_runtime_tick(&runtime, now_us + 59000U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_TALKING);
    CHECK(call_runtime_tick(&runtime, now_us + 60000U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 0U);
    const call_runtime_stats_t *stats = call_runtime_get_stats(&runtime);
    CHECK(stats != NULL);
    CHECK(stats->valid_numbers == 1U && stats->pickups_confirmed == 1U &&
          stats->hangups_confirmed == 1U);
    return 0;
}

static int test_invalid_number_never_opens_matrix(void)
{
    call_runtime_t runtime;
    matrix_manager_t matrix;
    fake_hw_t fake;
    memset(&fake, 0, sizeof(fake));
    CHECK(setup(&runtime, &matrix, &fake) == CALL_RUNTIME_OK);

    bool level = false;
    int64_t now_us = 1000;
    emit_address(&runtime, 81U, &now_us, &level, 0U);
    CHECK(call_runtime_tick(&runtime, now_us + 200U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_tick(&runtime, now_us + 80200U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 0U);
    CHECK(call_runtime_get_stats(&runtime)->invalid_number_frames == 1U);
    return 0;
}

static int test_event_loss_during_active_call_latches_fault(void)
{
    call_runtime_t runtime;
    matrix_manager_t matrix;
    fake_hw_t fake;
    memset(&fake, 0, sizeof(fake));
    CHECK(setup(&runtime, &matrix, &fake) == CALL_RUNTIME_OK);

    bool level = false;
    int64_t now_us = 1000;
    emit_address(&runtime, 8U, &now_us, &level, 0U);
    CHECK(call_runtime_tick(&runtime, now_us + 80200U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_RINGING);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 8U);

    const bool current[2] = { false, false };
    CHECK(call_runtime_notify_event_loss(&runtime, current, now_us + 90000U) ==
          CALL_RUNTIME_ERR_FAULT);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_FAULT);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_FAULT);
    CHECK(matrix_manager_get_active_apartment(&matrix) == 0U);

    CHECK(call_runtime_reset_fault(&runtime, current, now_us + 91000U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_STANDBY);
    CHECK(matrix_manager_get_state(&matrix) == MATRIX_MANAGER_STATE_OPEN);
    return 0;
}

static int test_second_call_is_ignored_while_busy(void)
{
    call_runtime_t runtime;
    matrix_manager_t matrix;
    fake_hw_t fake;
    memset(&fake, 0, sizeof(fake));
    CHECK(setup(&runtime, &matrix, &fake) == CALL_RUNTIME_OK);

    bool level = false;
    int64_t now_us = 1000;
    emit_address(&runtime, 12U, &now_us, &level, 0U);
    CHECK(call_runtime_tick(&runtime, now_us + 80200U) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_state(&runtime) == CALL_STATE_RINGING);
    const unsigned writes = fake.writes;

    /* Input from the other LU line cannot change the active apartment route. */
    now_us += 1000U;
    const lu_decoder_edge_t other_line = { LU_DECODER_LINE_2, true, now_us };
    CHECK(call_runtime_feed_lu_edge(&runtime, &other_line) == CALL_RUNTIME_OK);
    CHECK(call_runtime_get_apartment(&runtime) == 12U);
    CHECK(fake.writes == writes);
    return 0;
}

int main(void)
{
    CHECK(test_full_flow_decoder_to_talking_to_hangup() == 0);
    CHECK(test_invalid_number_never_opens_matrix() == 0);
    CHECK(test_event_loss_during_active_call_latches_fault() == 0);
    CHECK(test_second_call_is_ignored_while_busy() == 0);
    puts("All call runtime tests passed.");
    return 0;
}
