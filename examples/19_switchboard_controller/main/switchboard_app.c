#include "switchboard_app.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

#include "levisw_mb_v1_2_pins.h"
#include "mc33879a.h"
#include "tle94112.h"

#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
#if !CONFIG_SWITCHBOARD_HARDWARE_PROFILE_APPROVED
#error "Физический режим запрещён: подтвердите аппаратный профиль в menuconfig."
#endif
#if !CONFIG_SWITCHBOARD_POWER_ON_AT_BOOT
#error "Физический режим требует явного разрешения включения шин +V1/+V2."
#endif
#if (CONFIG_SWITCHBOARD_ACTIVE_UNIT_DRIVE != 1) && \
    (CONFIG_SWITCHBOARD_ACTIVE_UNIT_DRIVE != 2)
#error "Для физического режима явно выберите LOW_SIDE или HIGH_SIDE."
#endif
#if !__has_include("levisw_mb_v1_2_apartment_map_generated.h")
#error "Нет утверждённой карты квартир. Сначала заполните протокол проверки и сгенерируйте заголовок карты."
#endif
#include "levisw_mb_v1_2_apartment_map_generated.h"
#endif

#ifndef CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ
#define CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ 1000000
#endif
#ifndef CONFIG_LEVISW_SPI_MC33879_FREQ_HZ
#define CONFIG_LEVISW_SPI_MC33879_FREQ_HZ 1000000
#endif

static const char *TAG = "SWITCHBOARD";

typedef struct {
    bool mux_enabled;
    uint8_t decade;
    uint8_t d_mask;
    uint8_t unit;
    matrix_unit_drive_t unit_drive;
    bool drivers_enabled;
} simulated_matrix_t;

static switchboard_app_t s_app;
static simulated_matrix_t s_simulated_matrix;

static matrix_manager_status_t simulated_set_mux(void *context,
                                                  uint8_t decade,
                                                  bool enable)
{
    simulated_matrix_t *sim = (simulated_matrix_t *)context;
    if ((sim == NULL) || (decade >= MATRIX_MANAGER_DOZEN_COUNT)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->decade = decade;
    sim->mux_enabled = enable;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t simulated_set_d_mask(void *context, uint8_t mask)
{
    simulated_matrix_t *sim = (simulated_matrix_t *)context;
    if (sim == NULL) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->d_mask = mask;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t simulated_set_all_units(void *context,
                                                        matrix_unit_drive_t drive)
{
    simulated_matrix_t *sim = (simulated_matrix_t *)context;
    if (sim == NULL) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->unit_drive = drive;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t simulated_set_unit(void *context,
                                                   uint8_t unit,
                                                   matrix_unit_drive_t drive)
{
    simulated_matrix_t *sim = (simulated_matrix_t *)context;
    if ((sim == NULL) || (unit >= MATRIX_MANAGER_UNIT_COUNT)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->unit = unit;
    sim->unit_drive = drive;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t simulated_set_drivers(void *context, bool enabled)
{
    simulated_matrix_t *sim = (simulated_matrix_t *)context;
    if (sim == NULL) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->drivers_enabled = enabled;
    return MATRIX_MANAGER_OK;
}

static void simulated_delay(void *context, uint32_t delay_us)
{
    (void)context;
    (void)delay_us;
}

static void call_state_changed(void *context,
                               call_state_t previous,
                               call_state_t current,
                               uint8_t apartment)
{
    switchboard_app_t *app = (switchboard_app_t *)context;
    ESP_LOGI(TAG, "FSM вызова: %d -> %d, квартира=%u",
             (int)previous, (int)current, (unsigned)apartment);

    if (current == CALL_STATE_RINGING) {
        if ((app != NULL) && app->physical_matrix_enabled) {
            ESP_LOGW(TAG,
                     "Маршрут квартиры %u подключён. Генератор напряжения звонка "
                     "и аудиотракт этим приложением пока не управляются.",
                     (unsigned)apartment);
        } else {
            ESP_LOGW(TAG,
                     "SIMULATION ONLY: запрошен маршрут квартиры %u; "
                     "физические выходы D/E не переключались.",
                     (unsigned)apartment);
        }
    } else if (current == CALL_STATE_TALKING) {
        ESP_LOGI(TAG,
                 "Снятие трубки подтверждено. Управление аналоговым аудиотрактом "
                 "не входит в текущую интеграцию.");
    } else if (current == CALL_STATE_FAULT) {
        ESP_LOGE(TAG, "Зафиксировано FAULT; автоматический повтор вызова запрещён.");
    }
}

static esp_err_t initialize_simulated_matrix(void)
{
    matrix_manager_hw_ops_t ops = {
        .context = &s_simulated_matrix,
        .set_mux = simulated_set_mux,
        .set_d_mask = simulated_set_d_mask,
        .set_all_units = simulated_set_all_units,
        .set_unit = simulated_set_unit,
        .set_drivers_enabled = simulated_set_drivers,
        .delay_us = simulated_delay,
    };

    matrix_manager_config_t config;
    matrix_manager_default_config(&config);
    /* Для симулятора полярность не воздействует на электрические выходы. */
    config.active_unit_drive = MATRIX_UNIT_LOW_SIDE;

    const matrix_manager_status_t init_status =
        matrix_manager_init(&s_app.matrix, &ops, &config);
    if (init_status != MATRIX_MANAGER_OK) {
        ESP_LOGE(TAG, "Не удалось инициализировать симулятор матрицы: %d",
                 (int)init_status);
        return ESP_FAIL;
    }

    matrix_manager_address_map_t map;
    matrix_manager_address_map_make_sequential_candidate(&map);
    map.verified = true;
    (void)snprintf(map.pcb_revision, sizeof(map.pcb_revision), "%s", "SIMULATED");
    (void)snprintf(map.harness_revision, sizeof(map.harness_revision), "%s", "NO-HARNESS");
    (void)snprintf(map.commissioning_record, sizeof(map.commissioning_record),
                   "%s", "SOFTWARE-SIMULATION-ONLY");

    const matrix_manager_status_t map_status =
        matrix_manager_set_address_map(&s_app.matrix, &map);
    if (map_status != MATRIX_MANAGER_OK) {
        ESP_LOGE(TAG, "Не удалось установить тестовую карту симулятора: %d",
                 (int)map_status);
        return ESP_FAIL;
    }

    s_app.physical_matrix_enabled = false;
    ESP_LOGW(TAG,
             "РЕЖИМ СИМУЛЯЦИИ: карта 1..80 существует только в ОЗУ симулятора; "
             "EN-DE и выходы матрицы не включаются.");
    return ESP_OK;
}

#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
static void physical_best_effort_shutdown(void)
{
    (void)levisw_bsp_adg1608_select(0U, false);

    if (s_app.mc.spi_dev != NULL) {
        (void)mc33879a_set_outputs(&s_app.mc, 0x00U, NULL);
    }
    if (s_app.tle != NULL) {
        (void)tle94112_set_all_hb_state(s_app.tle, TLE_HB_FLOATING);
    }

    (void)levisw_bsp_drivers_enable(false);
    (void)levisw_bsp_power_set(false, false, false, false);
}

static esp_err_t check_power_profile(void)
{
    bool power_good = false;
    float v1_v = 0.0f;
    float v2_v = 0.0f;

    esp_err_t err = levisw_bsp_power_good(&power_good);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось прочитать PGOOD: %s", esp_err_to_name(err));
        return err;
    }
    if (!power_good) {
        ESP_LOGE(TAG, "PGOOD=0: силовые драйверы не разрешаются.");
        return ESP_ERR_INVALID_STATE;
    }

    err = levisw_bsp_ina226_read(LEVISW_INA_V1, &v1_v, NULL, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка измерения +V1 через DD5: %s", esp_err_to_name(err));
        return err;
    }
    err = levisw_bsp_ina226_read(LEVISW_INA_V2, &v2_v, NULL, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка измерения +V2 через DD6: %s", esp_err_to_name(err));
        return err;
    }

    const float min_v = (float)CONFIG_SWITCHBOARD_RAIL_MIN_MV / 1000.0f;
    const float max_v = (float)CONFIG_SWITCHBOARD_RAIL_MAX_MV / 1000.0f;
    if ((v1_v < min_v) || (v1_v > max_v) ||
        (v2_v < min_v) || (v2_v > max_v)) {
        ESP_LOGE(TAG,
                 "Напряжение вне заданного профиля: +V1=%.3f В, +V2=%.3f В; "
                 "разрешённый диапазон %.3f..%.3f В.",
                 v1_v, v2_v, min_v, max_v);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Питание подтверждено: PGOOD=1, +V1=%.3f В, +V2=%.3f В",
             v1_v, v2_v);
    return ESP_OK;
}

static esp_err_t initialize_physical_matrix(void)
{
    esp_err_t err;
    uint8_t v1_wiper = 0U;
    uint8_t v2_wiper = 0U;
    const levisw_bsp_handles_t *handles = levisw_bsp_get_handles();

    if (handles == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Повторно фиксируем отключённое состояние до изменения уставок питания. */
    err = levisw_bsp_drivers_enable(false);
    if (err != ESP_OK) {
        goto fail;
    }
    err = levisw_bsp_power_set(false, false, false, false);
    if (err != ESP_OK) {
        goto fail;
    }

    err = levisw_bsp_pot_set_wiper(LEVISW_BUS_V1,
                                  (uint8_t)CONFIG_SWITCHBOARD_V1_WIPER);
    if (err != ESP_OK) {
        goto fail;
    }
    err = levisw_bsp_pot_set_wiper(LEVISW_BUS_V2,
                                  (uint8_t)CONFIG_SWITCHBOARD_V2_WIPER);
    if (err != ESP_OK) {
        goto fail;
    }

    err = levisw_bsp_pot_get_wiper(LEVISW_BUS_V1, &v1_wiper);
    if (err != ESP_OK) {
        goto fail;
    }
    err = levisw_bsp_pot_get_wiper(LEVISW_BUS_V2, &v2_wiper);
    if (err != ESP_OK) {
        goto fail;
    }
    if ((v1_wiper != (uint8_t)CONFIG_SWITCHBOARD_V1_WIPER) ||
        (v2_wiper != (uint8_t)CONFIG_SWITCHBOARD_V2_WIPER)) {
        ESP_LOGE(TAG, "Контрольное чтение DA2/DA3 не совпало с заданными уставками.");
        err = ESP_ERR_INVALID_STATE;
        goto fail;
    }

    ESP_LOGW(TAG,
             "Физический профиль разрешён пользователем: DA2=%u, DA3=%u. "
             "Включаются только шины +V1/+V2; абонентская панель должна быть отключена.",
             (unsigned)v1_wiper, (unsigned)v2_wiper);

    err = levisw_bsp_power_set(true, true, false, false);
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(100U));

    err = check_power_profile();
    if (err != ESP_OK) {
        goto fail;
    }

    /* EN-DE поднимается только после проверки PGOOD и измеренных шин питания. */
    err = levisw_bsp_drivers_enable(true);
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(15U));

    if (handles->spi_tle94112 == NULL) {
        err = ESP_ERR_INVALID_STATE;
        goto fail;
    }

    /*
     * BSP регистрирует TLE94112 без флага LSB-first. Удаляем этот дескриптор
     * и создаём новый через специализированный драйвер TLE94112, который
     * устанавливает SPI_DEVICE_BIT_LSBFIRST. Шина SPI2 остаётся инициализированной BSP.
     */
    err = spi_bus_remove_device(handles->spi_tle94112);
    if (err != ESP_OK) {
        goto fail;
    }

    const tle94112_config_t tle_config = {
        .spi_host = SPI2_HOST,
        .gpio_cs = LEVISW_PIN_SPI_CS1_TLE,
        .gpio_en = -1, /* Общим EN-DE управляет BSP/Matrix Manager. */
        .spi_freq_hz = CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ,
    };
    s_app.tle = tle94112_init(&tle_config);
    if (s_app.tle == NULL) {
        ESP_LOGE(TAG, "Не удалось инициализировать TLE94112EL.");
        err = ESP_FAIL;
        goto fail;
    }

    const mc33879a_config_t mc_config = {
        .spi_host = SPI2_HOST,
        .mosi_io_num = LEVISW_PIN_SPI_MOSI,
        .miso_io_num = LEVISW_PIN_SPI_MISO,
        .sclk_io_num = LEVISW_PIN_SPI_CLK,
        .cs_io_num = LEVISW_PIN_SPI_CS2_MC33,
        .en_io_num = -1, /* Не переключать общий EN-DE из драйвера MC33879A. */
        .clock_speed_hz = CONFIG_LEVISW_SPI_MC33879_FREQ_HZ,
        .init_spi_bus = false,
        .existing_spi_dev = handles->spi_mc33879,
    };
    err = mc33879a_init(&mc_config, &s_app.mc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось инициализировать MC33879A: %s", esp_err_to_name(err));
        goto fail;
    }

    matrix_manager_hw_ops_t ops;
    err = matrix_manager_espidf_make_ops(&s_app.physical_adapter,
                                         s_app.tle,
                                         &s_app.mc,
                                         &ops);
    if (err != ESP_OK) {
        goto fail;
    }

    matrix_manager_config_t matrix_config;
    matrix_manager_default_config(&matrix_config);
#if CONFIG_SWITCHBOARD_ACTIVE_UNIT_DRIVE == 1
    matrix_config.active_unit_drive = MATRIX_UNIT_LOW_SIDE;
#elif CONFIG_SWITCHBOARD_ACTIVE_UNIT_DRIVE == 2
    matrix_config.active_unit_drive = MATRIX_UNIT_HIGH_SIDE;
#else
#error "Не выбрана полярность активного выхода E."
#endif

    const matrix_manager_status_t matrix_status =
        matrix_manager_init(&s_app.matrix, &ops, &matrix_config);
    if (matrix_status != MATRIX_MANAGER_OK) {
        ESP_LOGE(TAG, "Matrix Manager не перешёл в безопасное состояние OPEN: %d",
                 (int)matrix_status);
        err = ESP_FAIL;
        goto fail;
    }

    if (!matrix_manager_address_map_validate(&levisw_mb_v1_2_apartment_map) ||
        !levisw_mb_v1_2_apartment_map.verified) {
        ESP_LOGE(TAG, "Сгенерированная адресная карта отсутствует или не прошла проверку.");
        err = ESP_ERR_INVALID_STATE;
        goto fail;
    }

    const matrix_manager_status_t map_status =
        matrix_manager_set_address_map(&s_app.matrix,
                                       &levisw_mb_v1_2_apartment_map);
    if (map_status != MATRIX_MANAGER_OK) {
        ESP_LOGE(TAG, "Matrix Manager отклонил адресную карту: %d", (int)map_status);
        err = ESP_FAIL;
        goto fail;
    }

    s_app.physical_matrix_enabled = true;
    ESP_LOGW(TAG,
             "Физический Matrix Manager инициализирован. Матрица находится в OPEN, "
             "EN-DE отключён до первого запроса маршрута.");
    return ESP_OK;

fail:
    physical_best_effort_shutdown();
    return err;
}
#endif

static void runtime_task(void *argument)
{
    switchboard_app_t *app = (switchboard_app_t *)argument;
    uint32_t last_bsp_dropped = levisw_bsp_lu_event_dropped_count();
    uint32_t last_decoder_dropped = 0U;
    uint32_t last_read_errors = 0U;

    for (;;) {
        switchboard_control_event_t control_event;
        while (xQueueReceive(app->control_queue, &control_event, 0U) == pdTRUE) {
            if (control_event.type == SWITCHBOARD_CONTROL_HARDWARE_FAULT) {
                ESP_LOGE(TAG, "Супервизор сообщил аппаратную ошибку, код=%d",
                         control_event.reason);
                const call_runtime_status_t fault_status =
                    call_runtime_notify_hardware_fault(&app->runtime,
                                                       esp_timer_get_time());
                if (fault_status != CALL_RUNTIME_ERR_FAULT) {
                    ESP_LOGE(TAG, "Не удалось зафиксировать аппаратную ошибку в FSM: %d",
                             (int)fault_status);
                }
            }
        }

        levisw_bsp_lu_event_t event;
        esp_err_t err = lu_event_dispatcher_decoder_receive(app->dispatcher,
                                                            &event, 10U);
        if (err == ESP_OK) {
            do {
                const lu_decoder_edge_t edge = {
                    .line = (event.line == LEVISW_LU_LINE_1)
                          ? LU_DECODER_LINE_1 : LU_DECODER_LINE_2,
                    .level = event.level,
                    .timestamp_us = event.timestamp_us,
                };
                const call_runtime_status_t feed_status =
                    call_runtime_feed_lu_edge(&app->runtime, &edge);
                if (feed_status != CALL_RUNTIME_OK &&
                    feed_status != CALL_RUNTIME_ERR_INVALID_ARG) {
                    ESP_LOGW(TAG, "Runtime отклонил фронт LU: %d", (int)feed_status);
                }
                err = lu_event_dispatcher_decoder_receive(app->dispatcher,
                                                          &event, 0U);
            } while (err == ESP_OK);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Ошибка очереди декодера LU: %s", esp_err_to_name(err));
        }

        lu_event_dispatcher_stats_t stats;
        lu_event_dispatcher_get_stats(app->dispatcher, &stats);
        const uint32_t bsp_dropped = levisw_bsp_lu_event_dropped_count();

        if ((bsp_dropped != last_bsp_dropped) ||
            (stats.decoder_dropped != last_decoder_dropped) ||
            (stats.bsp_read_errors != last_read_errors)) {
            const bool current_levels[LU_DECODER_LINE_COUNT] = {
                levisw_bsp_lu1_read(),
                levisw_bsp_lu2_read(),
            };
            ESP_LOGE(TAG,
                     "Потеря управляющего потока LU: BSP=%" PRIu32
                     ", decoder=%" PRIu32 ", read_errors=%" PRIu32,
                     bsp_dropped - last_bsp_dropped,
                     stats.decoder_dropped - last_decoder_dropped,
                     stats.bsp_read_errors - last_read_errors);

            const call_runtime_status_t loss_status =
                call_runtime_notify_event_loss(&app->runtime,
                                               current_levels,
                                               esp_timer_get_time());
            if (loss_status != CALL_RUNTIME_OK &&
                loss_status != CALL_RUNTIME_ERR_FAULT) {
                ESP_LOGE(TAG, "Ошибка восстановления синхронизации LU: %d",
                         (int)loss_status);
            }

            last_bsp_dropped = bsp_dropped;
            last_decoder_dropped = stats.decoder_dropped;
            last_read_errors = stats.bsp_read_errors;
        }

        const call_runtime_status_t tick_status =
            call_runtime_tick(&app->runtime, esp_timer_get_time());
        if (tick_status == CALL_RUNTIME_ERR_FAULT) {
            ESP_LOGE(TAG, "Runtime находится в FAULT; требуется отдельное подтверждённое восстановление.");
        } else if (tick_status != CALL_RUNTIME_OK) {
            ESP_LOGW(TAG, "Ошибка шага Runtime: %d", (int)tick_status);
        }
    }
}

static void monitor_task(void *argument)
{
    switchboard_app_t *app = (switchboard_app_t *)argument;
    for (;;) {
        levisw_bsp_lu_event_t event;
        const esp_err_t err =
            lu_event_dispatcher_monitor_receive(app->dispatcher, &event, 1000U);
        if (err == ESP_OK) {
            ESP_LOGD(TAG, "LU%u raw=%u, t=%" PRId64 " мкс",
                     (unsigned)event.line,
                     event.level ? 1U : 0U,
                     event.timestamp_us);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Ошибка диагностической очереди LU: %s",
                     esp_err_to_name(err));
        }
    }
}

static void supervisor_task(void *argument)
{
    switchboard_app_t *app = (switchboard_app_t *)argument;
#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
    bool fault_already_queued = false;
#endif

    for (;;) {
#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
        if (app->physical_matrix_enabled && !fault_already_queued) {
            int fault_reason = 0;
            esp_err_t err = check_power_profile();
            if (err != ESP_OK) {
                fault_reason = (int)err;
            }

            if ((fault_reason == 0) && (app->tle != NULL)) {
                uint8_t sys_diag = 0U;
                err = tle94112_get_sys_diag1(app->tle, &sys_diag);
                if (err != ESP_OK) {
                    fault_reason = (int)err;
                } else if ((sys_diag & 0xFCU) != 0U) {
                    /* SPI_ERR, LE, VS_UV, VS_OV, NPOR и TSD требуют реакции супервизора. */
                    fault_reason = (int)sys_diag;
                }

                uint16_t overcurrent_mask = 0U;
                if ((fault_reason == 0) &&
                    (tle94112_get_overcurrent(app->tle, &overcurrent_mask) != ESP_OK)) {
                    fault_reason = (int)ESP_FAIL;
                } else if ((fault_reason == 0) && (overcurrent_mask != 0U)) {
                    fault_reason = (int)overcurrent_mask;
                }
            }

            if (fault_reason != 0) {
                const switchboard_control_event_t control = {
                    .type = SWITCHBOARD_CONTROL_HARDWARE_FAULT,
                    .reason = fault_reason,
                };
                if (xQueueSend(app->control_queue, &control, 0U) == pdTRUE) {
                    fault_already_queued = true;
                    ESP_LOGE(TAG, "Супервизор поставил аппаратную ошибку в очередь: %d",
                             fault_reason);
                }
            }
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(500U));
    }
}

esp_err_t switchboard_app_start(void)
{
    memset(&s_app, 0, sizeof(s_app));

    ESP_LOGI(TAG, "Этап 1/5: инициализация BSP платы");
    esp_err_t err = levisw_bsp_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка инициализации BSP: %s", esp_err_to_name(err));
        return err;
    }

#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
    ESP_LOGI(TAG, "Этап 2/5: запуск физического Matrix Manager");
    err = initialize_physical_matrix();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Физическая матрица не запущена. Выходы оставляются отключёнными.");
        return err;
    }
#else
    ESP_LOGI(TAG, "Этап 2/5: запуск программного симулятора Matrix Manager");
    err = initialize_simulated_matrix();
    if (err != ESP_OK) {
        return err;
    }
#endif

    ESP_LOGI(TAG, "Этап 3/5: инициализация Runtime/FSM");
    call_runtime_config_t runtime_config;
    call_runtime_default_config(&runtime_config);

    const bool initial_levels[LU_DECODER_LINE_COUNT] = {
        levisw_bsp_lu1_read(),
        levisw_bsp_lu2_read(),
    };
    err = call_runtime_init(&s_app.runtime,
                            &s_app.matrix,
                            &runtime_config,
                            initial_levels,
                            esp_timer_get_time(),
                            call_state_changed,
                            &s_app);
    if (err != CALL_RUNTIME_OK) {
        ESP_LOGE(TAG, "Ошибка инициализации Call Runtime: %d", (int)err);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Этап 4/5: запуск диспетчера LU и очереди управления");
    lu_event_dispatcher_config_t dispatcher_config;
    lu_event_dispatcher_default_config(&dispatcher_config);
    err = lu_event_dispatcher_start(&dispatcher_config, &s_app.dispatcher);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось запустить LU Event Dispatcher: %s",
                 esp_err_to_name(err));
        return err;
    }

    s_app.control_queue = xQueueCreate(4U, sizeof(switchboard_control_event_t));
    if (s_app.control_queue == NULL) {
        (void)lu_event_dispatcher_stop(s_app.dispatcher, 250U);
        s_app.dispatcher = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Этап 5/5: запуск задач Runtime, мониторинга и супервизора");
    TaskHandle_t runtime_handle = NULL;

    if (xTaskCreate(runtime_task, "switchboard_runtime", 6144U,
                    &s_app, 6U, &runtime_handle) != pdPASS) {
        (void)lu_event_dispatcher_stop(s_app.dispatcher, 250U);
        s_app.dispatcher = NULL;
        vQueueDelete(s_app.control_queue);
        s_app.control_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(supervisor_task, "switchboard_supervisor", 4096U,
                    &s_app, 5U, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Не удалось создать задачу аппаратного супервизора.");
#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
        if (s_app.physical_matrix_enabled) {
            vTaskDelete(runtime_handle);
            physical_best_effort_shutdown();
            (void)lu_event_dispatcher_stop(s_app.dispatcher, 250U);
            s_app.dispatcher = NULL;
            vQueueDelete(s_app.control_queue);
            s_app.control_queue = NULL;
            return ESP_ERR_NO_MEM;
        }
#endif
    }

    if (xTaskCreate(monitor_task, "switchboard_lu_monitor", 3072U,
                    &s_app, 3U, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Не удалось создать задачу диагностического монитора LU; FSM продолжит работу.");
    }

    s_app.initialized = true;
    ESP_LOGI(TAG, "Контроллер запущен. Физическая матрица: %s",
             s_app.physical_matrix_enabled ? "РАЗРЕШЕНА ПО ПРОВЕРЕННОМУ ПРОФИЛЮ"
                                           : "ОТКЛЮЧЕНА (симуляция)");
    return ESP_OK;
}
