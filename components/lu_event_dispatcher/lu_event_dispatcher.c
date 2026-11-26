#include "lu_event_dispatcher.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define LU_DISPATCHER_POLL_MS 20U

struct lu_event_dispatcher {
    QueueHandle_t decoder_queue;
    QueueHandle_t monitor_queue;
    SemaphoreHandle_t stopped;
    TaskHandle_t task;
    atomic_bool stop_requested;
    atomic_uint received;
    atomic_uint decoder_dropped;
    atomic_uint monitor_dropped;
    atomic_uint bsp_read_errors;
};

static TickType_t timeout_to_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == UINT32_MAX) {
        return portMAX_DELAY;
    }
    if (timeout_ms == 0U) {
        return 0;
    }

    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    return (ticks == 0U) ? 1U : ticks;
}

void lu_event_dispatcher_default_config(lu_event_dispatcher_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->decoder_queue_length = 64U;
    config->monitor_queue_length = 32U;
    config->task_stack_size = 3072U;
    config->task_priority = 5U;
}

static void dispatcher_task(void *arg)
{
    lu_event_dispatcher_handle_t dispatcher = (lu_event_dispatcher_handle_t)arg;

    while (!atomic_load_explicit(&dispatcher->stop_requested, memory_order_acquire)) {
        levisw_bsp_lu_event_t event;
        const esp_err_t err = levisw_bsp_lu_event_receive(&event, LU_DISPATCHER_POLL_MS);

        if (atomic_load_explicit(&dispatcher->stop_requested, memory_order_acquire)) {
            break;
        }

        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }
        if (err != ESP_OK) {
            atomic_fetch_add_explicit(&dispatcher->bsp_read_errors, 1U, memory_order_relaxed);
            vTaskDelay(1U);
            continue;
        }

        atomic_fetch_add_explicit(&dispatcher->received, 1U, memory_order_relaxed);

        /* Preserve every raw transition independently for the decoder and the UI/logger. */
        if (xQueueSend(dispatcher->decoder_queue, &event, 0U) != pdTRUE) {
            atomic_fetch_add_explicit(&dispatcher->decoder_dropped, 1U, memory_order_relaxed);
        }
        if (xQueueSend(dispatcher->monitor_queue, &event, 0U) != pdTRUE) {
            atomic_fetch_add_explicit(&dispatcher->monitor_dropped, 1U, memory_order_relaxed);
        }
    }

    /* Signal completion before self-deletion; the owner frees context only after this. */
    (void)xSemaphoreGive(dispatcher->stopped);
    vTaskDelete(NULL);
}

esp_err_t lu_event_dispatcher_start(const lu_event_dispatcher_config_t *config,
                                    lu_event_dispatcher_handle_t *out_dispatcher)
{
    if ((config == NULL) || (out_dispatcher == NULL) ||
        (config->decoder_queue_length == 0U) ||
        (config->monitor_queue_length == 0U) ||
        (config->task_stack_size == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_dispatcher = NULL;

    lu_event_dispatcher_handle_t dispatcher = calloc(1U, sizeof(*dispatcher));
    if (dispatcher == NULL) {
        return ESP_ERR_NO_MEM;
    }

    dispatcher->decoder_queue = xQueueCreate(config->decoder_queue_length,
                                              sizeof(levisw_bsp_lu_event_t));
    dispatcher->monitor_queue = xQueueCreate(config->monitor_queue_length,
                                              sizeof(levisw_bsp_lu_event_t));
    dispatcher->stopped = xSemaphoreCreateBinary();
    if ((dispatcher->decoder_queue == NULL) ||
        (dispatcher->monitor_queue == NULL) ||
        (dispatcher->stopped == NULL)) {
        if (dispatcher->decoder_queue != NULL) {
            vQueueDelete(dispatcher->decoder_queue);
        }
        if (dispatcher->monitor_queue != NULL) {
            vQueueDelete(dispatcher->monitor_queue);
        }
        if (dispatcher->stopped != NULL) {
            vSemaphoreDelete(dispatcher->stopped);
        }
        free(dispatcher);
        return ESP_ERR_NO_MEM;
    }

    atomic_init(&dispatcher->stop_requested, false);
    atomic_init(&dispatcher->received, 0U);
    atomic_init(&dispatcher->decoder_dropped, 0U);
    atomic_init(&dispatcher->monitor_dropped, 0U);
    atomic_init(&dispatcher->bsp_read_errors, 0U);

    BaseType_t created = xTaskCreate(dispatcher_task, "lu_dispatcher",
                                     config->task_stack_size, dispatcher,
                                     config->task_priority, &dispatcher->task);
    if (created != pdPASS) {
        vQueueDelete(dispatcher->decoder_queue);
        vQueueDelete(dispatcher->monitor_queue);
        vSemaphoreDelete(dispatcher->stopped);
        free(dispatcher);
        return ESP_ERR_NO_MEM;
    }

    *out_dispatcher = dispatcher;
    return ESP_OK;
}

static esp_err_t receive_from_queue(QueueHandle_t queue,
                                    levisw_bsp_lu_event_t *event,
                                    uint32_t timeout_ms)
{
    if ((queue == NULL) || (event == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    return (xQueueReceive(queue, event, timeout_to_ticks(timeout_ms)) == pdTRUE)
         ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t lu_event_dispatcher_decoder_receive(lu_event_dispatcher_handle_t dispatcher,
                                              levisw_bsp_lu_event_t *event,
                                              uint32_t timeout_ms)
{
    if (dispatcher == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return receive_from_queue(dispatcher->decoder_queue, event, timeout_ms);
}

esp_err_t lu_event_dispatcher_monitor_receive(lu_event_dispatcher_handle_t dispatcher,
                                              levisw_bsp_lu_event_t *event,
                                              uint32_t timeout_ms)
{
    if (dispatcher == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return receive_from_queue(dispatcher->monitor_queue, event, timeout_ms);
}

void lu_event_dispatcher_get_stats(lu_event_dispatcher_handle_t dispatcher,
                                   lu_event_dispatcher_stats_t *stats)
{
    if ((dispatcher == NULL) || (stats == NULL)) {
        return;
    }

    stats->received = atomic_load_explicit(&dispatcher->received, memory_order_relaxed);
    stats->decoder_dropped = atomic_load_explicit(&dispatcher->decoder_dropped, memory_order_relaxed);
    stats->monitor_dropped = atomic_load_explicit(&dispatcher->monitor_dropped, memory_order_relaxed);
    stats->bsp_read_errors = atomic_load_explicit(&dispatcher->bsp_read_errors, memory_order_relaxed);
}

esp_err_t lu_event_dispatcher_stop(lu_event_dispatcher_handle_t dispatcher,
                                   uint32_t timeout_ms)
{
    if (dispatcher == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    atomic_store_explicit(&dispatcher->stop_requested, true, memory_order_release);
    if (xSemaphoreTake(dispatcher->stopped, timeout_to_ticks(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    vQueueDelete(dispatcher->decoder_queue);
    vQueueDelete(dispatcher->monitor_queue);
    vSemaphoreDelete(dispatcher->stopped);
    free(dispatcher);
    return ESP_OK;
}
