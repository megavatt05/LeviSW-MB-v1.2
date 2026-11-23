#ifndef MATRIX_MANAGER_ESP_IDF_H
#define MATRIX_MANAGER_ESP_IDF_H

#include "esp_err.h"
#include "matrix_manager.h"
#include "tle94112.h"
#include "mc33879a.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Adapter context holds borrowed driver handles. The application must keep both
 * drivers alive for as long as the Matrix Manager is initialized. No ownership
 * or deinitialization is transferred to this adapter.
 */
typedef struct {
    tle94112_handle_t tle;
    mc33879a_handle_t *mc;
} matrix_manager_espidf_t;

/* Fill core hardware operations; call only after BSP, TLE and MC are initialized. */
esp_err_t matrix_manager_espidf_make_ops(matrix_manager_espidf_t *adapter,
                                         tle94112_handle_t tle,
                                         mc33879a_handle_t *mc,
                                         matrix_manager_hw_ops_t *out_ops);

#ifdef __cplusplus
}
#endif

#endif /* MATRIX_MANAGER_ESP_IDF_H */
