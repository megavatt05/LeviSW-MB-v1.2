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
 * Контекст адаптера содержит заимствованные дескрипторы драйверов. Приложение должно сохранять оба
 * драйвера действующими всё время, пока инициализирован Matrix Manager. Владение
 * ресурсами и обязанности по деинициализации этому адаптеру не передаются.
 */
typedef struct {
    tle94112_handle_t tle;
    mc33879a_handle_t *mc;
} matrix_manager_espidf_t;

/* Заполнить основные аппаратные операции; вызывать только после инициализации BSP, TLE и MC. */
esp_err_t matrix_manager_espidf_make_ops(matrix_manager_espidf_t *adapter,
                                         tle94112_handle_t tle,
                                         mc33879a_handle_t *mc,
                                         matrix_manager_hw_ops_t *out_ops);

#ifdef __cplusplus
}
#endif

#endif /* Защита от повторного включения заголовочного файла. */
