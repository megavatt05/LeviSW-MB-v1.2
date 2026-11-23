#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "levisw_mb_v1_2_bsp.h"
#include "lu_event_dispatcher.h"
#include "call_runtime.h"

static const char *TAG = "CALL_RUNTIME_MONITOR";
static lu_event_dispatcher_handle_t s_dispatcher;
static call_runtime_t s_runtime;

typedef struct {
    bool mux_enabled;
    uint8_t decade;
    uint8_t d_mask;
    uint8_t unit;
    matrix_unit_drive_t unit_drive;
    bool drivers_enabled;
} matrix_sim_t;

static matrix_sim_t s_matrix_sim;

static matrix_manager_status_t sim_mux(void *ctx, uint8_t decade, bool enable)
{
    matrix_sim_t *sim = (matrix_sim_t *)ctx;
    sim->decade = decade;
    sim->mux_enabled = enable;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_d_mask(void *ctx, uint8_t mask)
{
    matrix_sim_t *sim = (matrix_sim_t *)ctx;
    sim->d_mask = mask;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_all_units(void *ctx, matrix_unit_drive_t drive)
{
    matrix_sim_t *sim = (matrix_sim_t *)ctx;
    sim->unit_drive = drive;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_unit(void *ctx, uint8_t unit, matrix_unit_drive_t drive)
{
    matrix_sim_t *sim = (matrix_sim_t *)ctx;
    sim->unit = unit;
    sim->unit_drive = drive;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_drivers(void *ctx, bool enabled)
{
    matrix_sim_t *sim = (matrix_sim_t *)ctx;
    sim->drivers_enabled = enabled;
    return MATRIX_MANAGER_OK;
}

static void sim_delay(void *ctx, uint32_t delay_us)
{
    (void)ctx;
    (void)delay_us;
}

static void call_state_changed(void *ctx,
                               call_state_t previous,
                               call_state_t current,
                               uint8_t apartment)
{
    (void)ctx;
    ESP_LOGI(TAG, "CALL FSM: %d -> %d, apartment=%u",
             (int)previous, (int)current, (unsigned)apartment);

    if (current == CALL_STATE_RINGING) {
        const uint8_t index = (uint8_t)(apartment - 1U);
        ESP_LOGI(TAG,
                 "SIMULATION ONLY: flat %u => D%u x E%u; no electrical outputs changed",
                 (unsigned)apartment,
                 (unsigned)(index / 10U),
                 (unsigned)(index % 10U));
    }
    if (current == CALL_STATE_TALKING) {
        ESP_LOGI(TAG, "SIMULATION ONLY: handset pickup verified");
    }
    if (current == CALL_STATE_FAULT) {
        ESP_LOGE(TAG, "CALL FSM fault latched; simulated route is open");
    }
}

static void monitor_task(void *arg)
{
    (void)arg;
    for (;;) {
        levisw_bsp_lu_event_t event;
        if (lu_event_dispatcher_monitor_receive(s_dispatcher, &event, 100U) == ESP_OK) {
            ESP_LOGD(TAG, "LU%u raw level=%u @ %" PRId64 " us",
                     (unsigned)event.line, event.level ? 1U : 0U,
                     event.timestamp_us);
        }
    }
}

static void runtime_task(void *arg)
{
    (void)arg;
    uint32_t last_bsp_dropped = 0U;
    uint32_t last_decoder_dropped = 0U;
    uint32_t last_read_errors = 0U;

    for (;;) {
        levisw_bsp_lu_event_t event;
        esp_err_t err = lu_event_dispatcher_decoder_receive(s_dispatcher,
                                                               &event, 10U);
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Decoder queue error: %s", esp_err_to_name(err));
        }

        /* Drain timestamped edges before polling the decoder burst timeout. */
        while (err == ESP_OK) {
            const lu_decoder_edge_t edge = {
                .line = (event.line == LEVISW_LU_LINE_1)
                      ? LU_DECODER_LINE_1 : LU_DECODER_LINE_2,
                .level = event.level,
                .timestamp_us = event.timestamp_us,
            };
            const call_runtime_status_t feed_status =
                call_runtime_feed_lu_edge(&s_runtime, &edge);
            if (feed_status != CALL_RUNTIME_OK) {
                ESP_LOGW(TAG, "LU feed status=%d", (int)feed_status);
            }
            err = lu_event_dispatcher_decoder_receive(s_dispatcher, &event, 0U);
        }

        lu_event_dispatcher_stats_t dispatch_stats;
        lu_event_dispatcher_get_stats(s_dispatcher, &dispatch_stats);
        const uint32_t bsp_dropped = levisw_bsp_lu_event_dropped_count();

        if ((bsp_dropped != last_bsp_dropped) ||
            (dispatch_stats.decoder_dropped != last_decoder_dropped) ||
            (dispatch_stats.bsp_read_errors != last_read_errors)) {
            const bool current_levels[2] = {
                levisw_bsp_lu1_read(),
                levisw_bsp_lu2_read(),
            };
            ESP_LOGE(TAG, "LU stream loss: BSP=%" PRIu32 ", decoder=%" PRIu32
                          ", read_errors=%" PRIu32,
                     bsp_dropped - last_bsp_dropped,
                     dispatch_stats.decoder_dropped - last_decoder_dropped,
                     dispatch_stats.bsp_read_errors - last_read_errors);
            (void)call_runtime_notify_event_loss(&s_runtime, current_levels,
                                                 esp_timer_get_time());
            last_bsp_dropped = bsp_dropped;
            last_decoder_dropped = dispatch_stats.decoder_dropped;
            last_read_errors = dispatch_stats.bsp_read_errors;
        }

        const call_runtime_status_t tick_status =
            call_runtime_tick(&s_runtime, esp_timer_get_time());
        if (tick_status == CALL_RUNTIME_ERR_FAULT) {
            ESP_LOGE(TAG, "Runtime is in FAULT; explicit supervisor reset is required");
        } else if (tick_status != CALL_RUNTIME_OK) {
            ESP_LOGW(TAG, "Runtime tick status=%d", (int)tick_status);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "LeviSW-MB v1.2 — Call Runtime software integration monitor");
    ESP_LOGW(TAG, "MATRIX OUTPUTS ARE SIMULATED. No subscriber crosspoint is switched.");

    ESP_ERROR_CHECK(levisw_bsp_init());

    matrix_manager_hw_ops_t matrix_ops = {
        .context = &s_matrix_sim,
        .set_mux = sim_mux,
        .set_d_mask = sim_d_mask,
        .set_all_units = sim_all_units,
        .set_unit = sim_unit,
        .set_drivers_enabled = sim_drivers,
        .delay_us = sim_delay,
    };
    matrix_manager_config_t matrix_config;
    matrix_manager_default_config(&matrix_config);
    matrix_config.active_unit_drive = MATRIX_UNIT_LOW_SIDE;

    matrix_manager_t matrix;
    const matrix_manager_status_t matrix_status =
        matrix_manager_init(&matrix, &matrix_ops, &matrix_config);
    ESP_ERROR_CHECK(matrix_status == MATRIX_MANAGER_OK ? ESP_OK : ESP_FAIL);

    matrix_manager_address_map_t simulated_map;
    matrix_manager_address_map_make_sequential_candidate(&simulated_map);
    simulated_map.verified = true;
    (void)snprintf(simulated_map.pcb_revision, sizeof(simulated_map.pcb_revision), "%s", "SIMULATED");
    (void)snprintf(simulated_map.harness_revision, sizeof(simulated_map.harness_revision), "%s", "NO-HARNESS");
    (void)snprintf(simulated_map.commissioning_record, sizeof(simulated_map.commissioning_record),
                   "%s", "SOFTWARE-SIMULATION-ONLY");
    ESP_ERROR_CHECK(matrix_manager_set_address_map(&matrix, &simulated_map) ==
                    MATRIX_MANAGER_OK ? ESP_OK : ESP_FAIL);

    call_runtime_config_t runtime_config;
    call_runtime_default_config(&runtime_config);

    const bool initial_levels[2] = {
        levisw_bsp_lu1_read(),
        levisw_bsp_lu2_read(),
    };
    const call_runtime_status_t runtime_status =
        call_runtime_init(&s_runtime, &matrix, &runtime_config,
                          initial_levels, esp_timer_get_time(),
                          call_state_changed, NULL);
    ESP_ERROR_CHECK(runtime_status == CALL_RUNTIME_OK ? ESP_OK : ESP_FAIL);

    lu_event_dispatcher_config_t dispatcher_config;
    lu_event_dispatcher_default_config(&dispatcher_config);
    ESP_ERROR_CHECK(lu_event_dispatcher_start(&dispatcher_config, &s_dispatcher));

    if (xTaskCreate(runtime_task, "call_runtime", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create runtime task");
        (void)lu_event_dispatcher_stop(s_dispatcher, 250U);
        s_dispatcher = NULL;
        (void)levisw_bsp_deinit();
        return;
    }
    if (xTaskCreate(monitor_task, "lu_monitor", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create LU monitor task");
    }

    ESP_LOGI(TAG, "Runtime monitor started: LU edges are decoded and passed to FSM.");
    ESP_LOGI(TAG, "This target never powers the driver rails or enables EN-DE.");
}
