#ifndef CALL_RUNTIME_H
#define CALL_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "call_controller.h"
#include "handset_detector.h"
#include "lu_decoder.h"
#include "matrix_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CALL_RUNTIME_OK = 0,
    CALL_RUNTIME_ERR_INVALID_ARG = -1,
    CALL_RUNTIME_ERR_INVALID_STATE = -2,
    CALL_RUNTIME_ERR_FAULT = -3,
} call_runtime_status_t;

typedef struct {
    lu_decoder_config_t decoder;
    handset_detector_config_t handset;
    call_controller_config_t controller;
} call_runtime_config_t;

typedef struct {
    uint32_t valid_numbers;
    uint32_t invalid_number_frames;
    uint32_t calls_ignored_while_busy;
    uint32_t handset_candidates;
    uint32_t pickups_confirmed;
    uint32_t hangups_confirmed;
    uint32_t event_stream_losses;
} call_runtime_stats_t;

typedef struct {
    lu_decoder_t decoder;
    handset_detector_t handset;
    call_controller_t controller;
    call_runtime_config_t config;
    call_runtime_stats_t stats;
    bool latest_levels[LU_DECODER_LINE_COUNT];
    int64_t latest_edge_timestamp_us[LU_DECODER_LINE_COUNT];
    lu_decoder_line_t active_line;
    bool active_line_valid;
    bool initialized;
} call_runtime_t;

void call_runtime_default_config(call_runtime_config_t *config);

/*
 * The caller owns the Matrix Manager and must provide current GPIO levels at
 * initialization. Runtime is single-task-owned; all feed/tick calls must be
 * serialized by the application's call task.
 */
call_runtime_status_t call_runtime_init(call_runtime_t *runtime,
                                        matrix_manager_t *matrix,
                                        const call_runtime_config_t *config,
                                        const bool initial_levels[LU_DECODER_LINE_COUNT],
                                        int64_t now_us,
                                        call_state_changed_fn on_state_changed,
                                        void *callback_context);

/* Feed raw, timestamped LU edges from the single LU Event Dispatcher consumer. */
call_runtime_status_t call_runtime_feed_lu_edge(call_runtime_t *runtime,
                                                 const lu_decoder_edge_t *edge);

/* Poll decoder, handset detector and Call Controller deadlines. */
call_runtime_status_t call_runtime_tick(call_runtime_t *runtime, int64_t now_us);

/*
 * Call when BSP/dispatcher reports lost events. Active call => latched FAULT
 * and Matrix Manager emergency stop; standby => decoder resynchronisation.
 */
call_runtime_status_t call_runtime_notify_event_loss(call_runtime_t *runtime,
                                                      const bool current_levels[LU_DECODER_LINE_COUNT],
                                                      int64_t now_us);

/* Explicit recovery after an external supervisor has cleared the underlying fault. */
call_runtime_status_t call_runtime_reset_fault(call_runtime_t *runtime,
                                                const bool current_levels[LU_DECODER_LINE_COUNT],
                                                int64_t now_us);

const call_runtime_stats_t *call_runtime_get_stats(const call_runtime_t *runtime);
call_state_t call_runtime_get_state(const call_runtime_t *runtime);
uint8_t call_runtime_get_apartment(const call_runtime_t *runtime);
int call_runtime_get_active_line(const call_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif /* CALL_RUNTIME_H */
