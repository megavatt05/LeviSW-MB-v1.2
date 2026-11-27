#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "call_runtime.h"
#include "lu_event_dispatcher.h"
#include "levisw_mb_v1_2_bsp.h"
#include "matrix_manager_adg_tle.h"

typedef enum {
    SWITCHBOARD_CONTROL_HARDWARE_FAULT = 1,
} switchboard_control_type_t;

typedef struct {
    switchboard_control_type_t type;
    int reason;
} switchboard_control_event_t;

typedef enum {
    SWITCHBOARD_RING_START = 1,
    SWITCHBOARD_RING_STOP = 2,
} switchboard_ring_command_t;

typedef struct {
    matrix_manager_t matrix;
    call_runtime_t runtime;
    lu_event_dispatcher_handle_t dispatcher;
    QueueHandle_t control_queue;
    QueueHandle_t ring_queue;

    matrix_manager_adg_tle_t matrix_adapter;
    tle94112_handle_t tle;

    bool physical_matrix_enabled;
    bool initialized;
} switchboard_app_t;

esp_err_t switchboard_app_start(void);
