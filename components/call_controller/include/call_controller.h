#ifndef CALL_CONTROLLER_H
#define CALL_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "matrix_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CALL_CONTROLLER_OK = 0,
    CALL_CONTROLLER_ERR_INVALID_ARG = -1,
    CALL_CONTROLLER_ERR_INVALID_STATE = -2,
    CALL_CONTROLLER_ERR_BUSY = -3,
    CALL_CONTROLLER_ERR_INVALID_NUMBER = -4,
    CALL_CONTROLLER_ERR_MATRIX = -5,
    CALL_CONTROLLER_ERR_FAULT = -6,
} call_controller_status_t;

typedef enum {
    CALL_STATE_STANDBY = 0,
    CALL_STATE_CAPTURE_NUMBER,
    CALL_STATE_VALIDATE_TARGET,
    CALL_STATE_ROUTE_PREPARE,
    CALL_STATE_RINGING,
    CALL_STATE_ANSWER_VERIFY,
    CALL_STATE_TALKING,
    CALL_STATE_HANGUP,
    CALL_STATE_FAULT,
} call_state_t;

typedef enum {
    CALL_EVENT_CALL_DETECTED = 0,
    CALL_EVENT_NUMBER_READY,
    CALL_EVENT_PICKUP_CANDIDATE,
    CALL_EVENT_PICKUP_CONFIRMED,
    CALL_EVENT_PICKUP_REJECTED,
    CALL_EVENT_HANGUP_DETECTED,
    CALL_EVENT_CANCEL,
    CALL_EVENT_HARDWARE_FAULT,
    CALL_EVENT_FAULT_RESET,
} call_event_type_t;

typedef struct {
    call_event_type_t type;
    uint8_t apartment;     /* Used only by NUMBER_READY; range is 1..80. */
    bool decoder_valid;    /* Must be true for a complete, valid decoder result. */
} call_event_t;

typedef struct {
    /* 0 disables the corresponding timeout until calibrated on the real panel. */
    uint32_t number_capture_timeout_ms;
    uint32_t ring_timeout_ms;
    uint32_t answer_verify_timeout_ms;
} call_controller_config_t;

typedef void (*call_state_changed_fn)(void *context,
                                      call_state_t previous,
                                      call_state_t current,
                                      uint8_t apartment);

typedef struct {
    matrix_manager_t *matrix;
    call_controller_config_t config;
    call_state_changed_fn on_state_changed;
    void *callback_context;
    call_state_t state;
    uint8_t apartment;
    uint64_t state_started_ms;
    call_controller_status_t last_error;
    bool initialized;
} call_controller_t;

void call_controller_default_config(call_controller_config_t *config);

/* The supplied Matrix Manager must already be initialized and safely OPEN. */
call_controller_status_t call_controller_init(call_controller_t *controller,
                                               matrix_manager_t *matrix,
                                               const call_controller_config_t *config,
                                               call_state_changed_fn on_state_changed,
                                               void *callback_context);

/* Process a semantic event from the protocol decoder / handset detector. */
call_controller_status_t call_controller_handle_event(call_controller_t *controller,
                                                       const call_event_t *event,
                                                       uint64_t now_ms);

/* Advance configured timeouts. Call periodically from the application task. */
call_controller_status_t call_controller_tick(call_controller_t *controller,
                                               uint64_t now_ms);

call_state_t call_controller_get_state(const call_controller_t *controller);
uint8_t call_controller_get_apartment(const call_controller_t *controller);
call_controller_status_t call_controller_get_last_error(const call_controller_t *controller);

#ifdef __cplusplus
}
#endif

#endif /* CALL_CONTROLLER_H */
