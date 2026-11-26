#ifndef LU_EVENT_DISPATCHER_H
#define LU_EVENT_DISPATCHER_H

#include <stdint.h>
#include "esp_err.h"
#include "levisw_mb_v1_2_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lu_event_dispatcher *lu_event_dispatcher_handle_t;

typedef struct {
    uint32_t decoder_queue_length;
    uint32_t monitor_queue_length;
    uint32_t task_stack_size;
    uint32_t task_priority;
} lu_event_dispatcher_config_t;

typedef struct {
    uint32_t received;
    uint32_t decoder_dropped;
    uint32_t monitor_dropped;
    uint32_t bsp_read_errors;
} lu_event_dispatcher_stats_t;

void lu_event_dispatcher_default_config(lu_event_dispatcher_config_t *config);

/* Starts the sole task that reads the BSP LU queue and fans out raw edges. */
esp_err_t lu_event_dispatcher_start(const lu_event_dispatcher_config_t *config,
                                    lu_event_dispatcher_handle_t *out_dispatcher);

/* Each output queue has one consumer. Do not call either receive API from multiple tasks. */
esp_err_t lu_event_dispatcher_decoder_receive(lu_event_dispatcher_handle_t dispatcher,
                                              levisw_bsp_lu_event_t *event,
                                              uint32_t timeout_ms);
esp_err_t lu_event_dispatcher_monitor_receive(lu_event_dispatcher_handle_t dispatcher,
                                              levisw_bsp_lu_event_t *event,
                                              uint32_t timeout_ms);

void lu_event_dispatcher_get_stats(lu_event_dispatcher_handle_t dispatcher,
                                   lu_event_dispatcher_stats_t *stats);

/* Stops the dispatcher and frees both subscriber queues. Stop consumers first. */
esp_err_t lu_event_dispatcher_stop(lu_event_dispatcher_handle_t dispatcher,
                                   uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* LU_EVENT_DISPATCHER_H */
