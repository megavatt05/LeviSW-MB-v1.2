/*
 * SPDX-FileCopyrightText: 2026 Albert
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tle94112_example_start(const char *task_name,
                                 uint32_t stack_size,
                                 UBaseType_t priority,
                                 BaseType_t core_id);

#ifdef __cplusplus
}
#endif
