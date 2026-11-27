#pragma once

#include "esp_err.h"
#include "matrix_manager.h"
#include "tle94112.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Физическая координатная схема Example 20:
 *   D0..D7 = выбор одного канала ADG1608BR через TCA9535 (A0..A2, SW-EN).
 *   E0..E9 = один из десяти полумостов TLE94112EL.
 *
 * MC33879APEKR2 намеренно отсутствует в этом адаптере.
 * ADG1608 является собственно селектором координаты D; поэтому операция
 * set_d_mask() лишь проверяет согласованность маски с выбранным ADG-каналом.
 */
typedef struct {
    tle94112_handle_t tle;
    uint8_t selected_decade;
    bool mux_enabled;
} matrix_manager_adg_tle_t;

esp_err_t matrix_manager_adg_tle_make_ops(
    matrix_manager_adg_tle_t *adapter,
    tle94112_handle_t tle,
    matrix_manager_hw_ops_t *out_ops);

#ifdef __cplusplus
}
#endif
