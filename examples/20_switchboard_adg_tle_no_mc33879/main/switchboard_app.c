#include "switchboard_app.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "driver/dac_cosine.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "levisw_mb_v1_2_pins.h"
#include "tle94112.h"

#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
#if !CONFIG_SWITCHBOARD_HARDWARE_PROFILE_APPROVED
#error "Физический режим запрещён: подтвердите профиль платы в menuconfig."
#endif
#if !CONFIG_SWITCHBOARD_ADG_IS_D_PATH_CONFIRMED
#error "Физический режим запрещён: подтвердите прозвонкой, что ADG1608 действительно формирует D0..D7 на квартирном жгуте. См. README / HW-021."
#endif
#if !CONFIG_SWITCHBOARD_MC33879_SAFE_WITH_SHARED_EN
#error "Физический режим запрещён: EN-DE общий у TLE94112 и MC33879. Подтвердите безопасное состояние MC33879 при EN-DE=HIGH либо его физическое отсутствие."
#endif
#if !CONFIG_SWITCHBOARD_POWER_ON_AT_BOOT
#error "Физический режим требует явного разрешения включения +V1/+V2."
#endif
#if (CONFIG_SWITCHBOARD_ACTIVE_UNIT_DRIVE != 1) && \
    (CONFIG_SWITCHBOARD_ACTIVE_UNIT_DRIVE != 2)
#error "Для физического режима явно выберите LOW_SIDE или HIGH_SIDE."
#endif
#if !__has_include("levisw_mb_v1_2_apartment_map_generated.h")
#error "Нет проверенной карты 80 квартир. Сначала заполните протокол и CSV."
#endif
#include "levisw_mb_v1_2_apartment_map_generated.h"
#endif

#ifndef CONFIG_SWITCHBOARD_RING_AMP_MASK
#define CONFIG_SWITCHBOARD_RING_AMP_MASK 0
#endif
#ifndef CONFIG_SWITCHBOARD_RING_TONE_HZ
#define CONFIG_SWITCHBOARD_RING_TONE_HZ 425
#endif
#ifndef CONFIG_SWITCHBOARD_RING_ON_MS
#define CONFIG_SWITCHBOARD_RING_ON_MS 1000
#endif
#ifndef CONFIG_SWITCHBOARD_RING_OFF_MS
#define CONFIG_SWITCHBOARD_RING_OFF_MS 4000
#endif
#ifndef CONFIG_SWITCHBOARD_ALARM_MONITOR_ENABLE
#define CONFIG_SWITCHBOARD_ALARM_MONITOR_ENABLE 1
#endif
#ifndef CONFIG_SWITCHBOARD_ALARM_FAULT_ENABLE
#define CONFIG_SWITCHBOARD_ALARM_FAULT_ENABLE 0
#endif
#ifndef CONFIG_SWITCHBOARD_ALARM_ACTIVE_HIGH
#define CONFIG_SWITCHBOARD_ALARM_ACTIVE_HIGH 1
#endif

static const char *TAG = "SWITCHBOARD";

typedef struct {
    bool mux_enabled;
    uint8_t decade;
    uint8_t unit;
    uint8_t d_mask;
    matrix_unit_drive_t unit_drive;
    bool drivers_enabled;
} simulated_matrix_t;

typedef struct {
    dac_cosine_handle_t sound1;
    dac_cosine_handle_t sound2;
    bool tone_ready;
    bool tone_running;
} ring_audio_t;

static switchboard_app_t s_app;
static simulated_matrix_t s_simulated_matrix;
static ring_audio_t s_ring_audio;

static const char *call_state_name(call_state_t state)
{
    switch (state) {
    case CALL_STATE_STANDBY: return "STANDBY";
    case CALL_STATE_CAPTURE_NUMBER: return "CAPTURE_NUMBER";
    case CALL_STATE_VALIDATE_TARGET: return "VALIDATE_TARGET";
    case CALL_STATE_ROUTE_PREPARE: return "ROUTE_PREPARE";
    case CALL_STATE_RINGING: return "RINGING";
    case CALL_STATE_ANSWER_VERIFY: return "ANSWER_VERIFY";
    case CALL_STATE_TALKING: return "TALKING";
    case CALL_STATE_HANGUP: return "HANGUP";
    case CALL_STATE_FAULT: return "FAULT";
    default: return "UNKNOWN";
    }
}

static void queue_ring_command(switchboard_ring_command_t command)
{
    if (s_app.ring_queue != NULL) {
        if (xQueueSend(s_app.ring_queue, &command, 0U) != pdTRUE) {
            ESP_LOGW(TAG, "Очередь управления тоном заполнена; команда %d потеряна",
                     (int)command);
        }
    }
}

static void call_state_changed(void *context,
                               call_state_t previous,
                               call_state_t current,
                               uint8_t apartment)
{
    (void)context;
    ESP_LOGI(TAG, "FSM: %s -> %s, квартира=%u",
             call_state_name(previous), call_state_name(current),
             (unsigned)apartment);

    if (current == CALL_STATE_RINGING) {
        queue_ring_command(SWITCHBOARD_RING_START);
        ESP_LOGI(TAG,
                 "ВЫЗОВ: маршрут квартиры %u подготовлен; включён цикл сигнала вызова",
                 (unsigned)apartment);
    } else if ((current == CALL_STATE_ANSWER_VERIFY) ||
               (current == CALL_STATE_TALKING) ||
               (current == CALL_STATE_HANGUP) ||
               (current == CALL_STATE_STANDBY) ||
               (current == CALL_STATE_FAULT)) {
        queue_ring_command(SWITCHBOARD_RING_STOP);
    }

    if (current == CALL_STATE_TALKING) {
        ESP_LOGI(TAG, "Трубка снята: остановить сигнал вызова, оставить маршрут активным");
    } else if (current == CALL_STATE_FAULT) {
        ESP_LOGE(TAG, "FAULT: вызов запрещён до явного восстановления");
    }
}

/* -------------------------------------------------------------------------- */
/* Программная модель матрицы — используется по умолчанию.                    */
/* -------------------------------------------------------------------------- */

static matrix_manager_status_t sim_set_decade(void *context,
                                              uint8_t decade,
                                              bool enable)
{
    simulated_matrix_t *sim = context;
    if ((sim == NULL) || (decade >= MATRIX_MANAGER_DOZEN_COUNT)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->decade = decade;
    sim->mux_enabled = enable;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_set_d_mask(void *context, uint8_t mask)
{
    simulated_matrix_t *sim = context;
    if (sim == NULL) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->d_mask = mask;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_set_all_e(void *context,
                                             matrix_unit_drive_t drive)
{
    simulated_matrix_t *sim = context;
    if (sim == NULL) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->unit_drive = drive;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_set_e(void *context,
                                         uint8_t unit,
                                         matrix_unit_drive_t drive)
{
    simulated_matrix_t *sim = context;
    if ((sim == NULL) || (unit >= MATRIX_MANAGER_UNIT_COUNT)) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->unit = unit;
    sim->unit_drive = drive;
    return MATRIX_MANAGER_OK;
}

static matrix_manager_status_t sim_set_driver_enable(void *context, bool enabled)
{
    simulated_matrix_t *sim = context;
    if (sim == NULL) {
        return MATRIX_MANAGER_ERR_INVALID_ARG;
    }
    sim->drivers_enabled = enabled;
    return MATRIX_MANAGER_OK;
}

static void sim_delay(void *context, uint32_t delay_us)
{
    (void)context;
    (void)delay_us;
}

static esp_err_t init_simulated_matrix(void)
{
    const matrix_manager_hw_ops_t ops = {
        .context = &s_simulated_matrix,
        .set_mux = sim_set_decade,
        .set_d_mask = sim_set_d_mask,
        .set_all_units = sim_set_all_e,
        .set_unit = sim_set_e,
        .set_drivers_enabled = sim_set_driver_enable,
        .delay_us = sim_delay,
    };

    matrix_manager_config_t config;
    matrix_manager_default_config(&config);
    config.active_unit_drive = MATRIX_UNIT_LOW_SIDE; /* Только RAM-симулятор. */

    matrix_manager_status_t status =
        matrix_manager_init(&s_app.matrix, &ops, &config);
    if (status != MATRIX_MANAGER_OK) {
        ESP_LOGE(TAG, "Matrix Manager simulator init failed: %d", (int)status);
        return ESP_FAIL;
    }

    matrix_manager_address_map_t map;
    matrix_manager_address_map_make_sequential_candidate(&map);
    map.verified = true;
    (void)snprintf(map.pcb_revision, sizeof(map.pcb_revision), "%s", "SIMULATOR");
    (void)snprintf(map.harness_revision, sizeof(map.harness_revision), "%s", "NO-HARNESS");
    (void)snprintf(map.commissioning_record, sizeof(map.commissioning_record),
                   "%s", "SOFTWARE-SIMULATION-ONLY");

    status = matrix_manager_set_address_map(&s_app.matrix, &map);
    if (status != MATRIX_MANAGER_OK) {
        ESP_LOGE(TAG, "Не удалось установить карту симулятора: %d", (int)status);
        return ESP_FAIL;
    }

    s_app.physical_matrix_enabled = false;
    ESP_LOGW(TAG,
             "SIMULATION ONLY: ADG1608/TLE94112 в приложении моделируются; "
             "EN-DE остаётся LOW, D/E физически не переключаются");
    return ESP_OK;
}

#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
/* -------------------------------------------------------------------------- */
/* Физическая матрица: D=ADG1608BR, E=TLE94112EL. MC33879 в коде отсутствует.  */
/* -------------------------------------------------------------------------- */

static esp_err_t check_power_profile(void)
{
    bool pg = false;
    float v1 = 0.0f;
    float v2 = 0.0f;

    esp_err_t err = levisw_bsp_power_good(&pg);
    if (err != ESP_OK) {
        return err;
    }
    if (!pg) {
        ESP_LOGE(TAG, "PGOOD=0");
        return ESP_ERR_INVALID_STATE;
    }

    err = levisw_bsp_ina226_read(LEVISW_INA_V1, &v1, NULL, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DD5 INA226 (+V1) недоступен: %s", esp_err_to_name(err));
        return err;
    }
    err = levisw_bsp_ina226_read(LEVISW_INA_V2, &v2, NULL, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DD6 INA226 (+V2) недоступен: %s", esp_err_to_name(err));
        return err;
    }

    const float min_v = (float)CONFIG_SWITCHBOARD_RAIL_MIN_MV / 1000.0f;
    const float max_v = (float)CONFIG_SWITCHBOARD_RAIL_MAX_MV / 1000.0f;
    if ((v1 < min_v) || (v1 > max_v) || (v2 < min_v) || (v2 > max_v)) {
        ESP_LOGE(TAG, "Напряжение вне профиля: +V1=%.3f В, +V2=%.3f В, пределы %.3f..%.3f В",
                 v1, v2, min_v, max_v);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Питание подтверждено: PGOOD=1, +V1=%.3f В, +V2=%.3f В", v1, v2);
    return ESP_OK;
}

static void physical_best_effort_shutdown(void)
{
    (void)levisw_bsp_adg1608_select(0U, false);

    if (s_app.tle != NULL) {
        for (uint8_t e = 0U; e < MATRIX_MANAGER_UNIT_COUNT; ++e) {
            const uint8_t hb = levisw_bsp_unit_to_tle_hb(e);
            if (hb > 0U) {
                (void)tle94112_set_hb_state(s_app.tle,
                                             (tle_hb_idx_t)(hb - 1U),
                                             TLE_HB_FLOATING);
            }
        }
    }

    (void)levisw_bsp_drivers_enable(false);
    (void)levisw_bsp_power_set(false, false, false, false);
}

static esp_err_t init_physical_matrix(void)
{
    const levisw_bsp_handles_t *handles = levisw_bsp_get_handles();
    if ((handles == NULL) || (handles->spi_tle94112 == NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = levisw_bsp_drivers_enable(false);
    if (err != ESP_OK) {
        return err;
    }
    err = levisw_bsp_adg1608_select(0U, false);
    if (err != ESP_OK) {
        return err;
    }
    err = levisw_bsp_power_set(false, false, false, false);
    if (err != ESP_OK) {
        return err;
    }

    /* Уставки силовых шин записываются до их включения. */
    err = levisw_bsp_pot_set_wiper(LEVISW_BUS_V1,
                                  (uint8_t)CONFIG_SWITCHBOARD_V1_WIPER);
    if (err != ESP_OK) goto fail;
    err = levisw_bsp_pot_set_wiper(LEVISW_BUS_V2,
                                  (uint8_t)CONFIG_SWITCHBOARD_V2_WIPER);
    if (err != ESP_OK) goto fail;

    uint8_t v1_wiper = 0U;
    uint8_t v2_wiper = 0U;
    err = levisw_bsp_pot_get_wiper(LEVISW_BUS_V1, &v1_wiper);
    if (err != ESP_OK) goto fail;
    err = levisw_bsp_pot_get_wiper(LEVISW_BUS_V2, &v2_wiper);
    if (err != ESP_OK) goto fail;
    if ((v1_wiper != (uint8_t)CONFIG_SWITCHBOARD_V1_WIPER) ||
        (v2_wiper != (uint8_t)CONFIG_SWITCHBOARD_V2_WIPER)) {
        err = ESP_ERR_INVALID_STATE;
        goto fail;
    }

    ESP_LOGW(TAG,
             "Включение +V1/+V2 разрешено Kconfig; квартирный жгут должен быть отключён");
    err = levisw_bsp_power_set(true, true, false, false);
    if (err != ESP_OK) goto fail;
    vTaskDelay(pdMS_TO_TICKS(100U));

    err = check_power_profile();
    if (err != ESP_OK) goto fail;

    /*
     * EN-DE общий для обоих драйверов на плате. В этом проекте
     * регистрируется только TLE94112; MC33879 не инициализируется и не вызывается.
     */
    err = levisw_bsp_drivers_enable(true);
    if (err != ESP_OK) goto fail;
    vTaskDelay(pdMS_TO_TICKS(15U));

    /* Удалить MSB-first BSP-дескриптор и передать CS-E специализированному LSB-first драйверу. */
    err = levisw_bsp_tle_release_spi_device();
    if (err != ESP_OK) goto fail;

    const tle94112_config_t tle_cfg = {
        .spi_host = SPI2_HOST,
        .gpio_cs = LEVISW_PIN_SPI_CS1_TLE,
        .gpio_en = -1, /* EN-DE управляется BSP / Matrix Manager. */
        .spi_freq_hz = CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ,
    };
    s_app.tle = tle94112_init(&tle_cfg);
    if (s_app.tle == NULL) {
        err = ESP_FAIL;
        goto fail;
    }

    matrix_manager_hw_ops_t ops;
    err = matrix_manager_adg_tle_make_ops(&s_app.matrix_adapter, s_app.tle, &ops);
    if (err != ESP_OK) goto fail;

    matrix_manager_config_t matrix_cfg;
    matrix_manager_default_config(&matrix_cfg);
#if CONFIG_SWITCHBOARD_ACTIVE_UNIT_DRIVE == 1
    matrix_cfg.active_unit_drive = MATRIX_UNIT_LOW_SIDE;
#else
    matrix_cfg.active_unit_drive = MATRIX_UNIT_HIGH_SIDE;
#endif

    matrix_manager_status_t mm_status =
        matrix_manager_init(&s_app.matrix, &ops, &matrix_cfg);
    if (mm_status != MATRIX_MANAGER_OK) {
        err = ESP_FAIL;
        goto fail;
    }

    if (!matrix_manager_address_map_validate(&levisw_mb_v1_2_apartment_map) ||
        !levisw_mb_v1_2_apartment_map.verified) {
        err = ESP_ERR_INVALID_STATE;
        goto fail;
    }

    mm_status = matrix_manager_set_address_map(&s_app.matrix,
                                               &levisw_mb_v1_2_apartment_map);
    if (mm_status != MATRIX_MANAGER_OK) {
        err = ESP_FAIL;
        goto fail;
    }

    s_app.physical_matrix_enabled = true;
    ESP_LOGW(TAG,
             "ФИЗИЧЕСКИЙ РЕЖИМ: D=ADG1608, E=TLE94112; "
             "матрица разомкнута, EN-DE LOW до первого запроса");
    return ESP_OK;

fail:
    physical_best_effort_shutdown();
    return err;
}
#endif /* CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX */

/* -------------------------------------------------------------------------- */
/* Тестовый аудиотон вызова на SOUND1/SOUND2. Это НЕ высоковольтный ring feed. */
/* -------------------------------------------------------------------------- */

static esp_err_t ring_audio_init(void)
{
#if CONFIG_SWITCHBOARD_ENABLE_DAC_RING
    const dac_cosine_config_t sound1_cfg = {
        .chan_id = LEVISW_DAC_CH_SOUND1,
        .freq_hz = CONFIG_SWITCHBOARD_RING_TONE_HZ,
        .clk_src = DAC_COSINE_CLK_SRC_DEFAULT,
        .atten = DAC_COSINE_ATTEN_DB_18,
        .phase = DAC_COSINE_PHASE_0,
        .offset = 0,
        .flags.force_set_freq = false,
    };
    esp_err_t err = dac_cosine_new_channel(&sound1_cfg, &s_ring_audio.sound1);
    if (err != ESP_OK) {
        return err;
    }

    dac_cosine_config_t sound2_cfg = sound1_cfg;
    sound2_cfg.chan_id = LEVISW_DAC_CH_SOUND2;
    sound2_cfg.phase = DAC_COSINE_PHASE_180;
    sound2_cfg.flags.force_set_freq = false;
    err = dac_cosine_new_channel(&sound2_cfg, &s_ring_audio.sound2);
    if (err != ESP_OK) {
        (void)dac_cosine_del_channel(s_ring_audio.sound1);
        s_ring_audio.sound1 = NULL;
        return err;
    }

    s_ring_audio.tone_ready = true;
    ESP_LOGW(TAG,
             "Тестовый тон DAC готов: %u Гц, ослабление 18 dB, SOUND1/SOUND2",
             (unsigned)CONFIG_SWITCHBOARD_RING_TONE_HZ);
#else
    ESP_LOGI(TAG, "DAC-звонок выключен Kconfig; FSM будет показывать RINGING в журнале");
#endif
    return ESP_OK;
}

static void ring_audio_set(bool enable)
{
#if CONFIG_SWITCHBOARD_ENABLE_DAC_RING
    if (!s_ring_audio.tone_ready || (enable == s_ring_audio.tone_running)) {
        return;
    }

    if (enable) {
        const uint8_t mask = (uint8_t)CONFIG_SWITCHBOARD_RING_AMP_MASK;
        const esp_err_t amp_err = levisw_bsp_amp_set(
            (mask & 0x01U) != 0U,
            (mask & 0x02U) != 0U,
            (mask & 0x04U) != 0U,
            false, false, false);
        if (amp_err != ESP_OK) {
            ESP_LOGE(TAG, "Не удалось задать ENAMP1..3: %s", esp_err_to_name(amp_err));
            return;
        }

        esp_err_t err = dac_cosine_start(s_ring_audio.sound1);
        if (err == ESP_OK) {
            err = dac_cosine_start(s_ring_audio.sound2);
        }
        if (err != ESP_OK) {
            (void)dac_cosine_stop(s_ring_audio.sound1);
            (void)levisw_bsp_amp_set(false, false, false, false, false, false);
            ESP_LOGE(TAG, "Не удалось запустить DAC-тон: %s", esp_err_to_name(err));
            return;
        }
        s_ring_audio.tone_running = true;
        ESP_LOGI(TAG, "SOUND1/SOUND2: тестовый тон ВКЛ");
    } else {
        (void)dac_cosine_stop(s_ring_audio.sound1);
        (void)dac_cosine_stop(s_ring_audio.sound2);
        (void)levisw_bsp_amp_set(false, false, false, false, false, false);
        s_ring_audio.tone_running = false;
        ESP_LOGI(TAG, "SOUND1/SOUND2: тестовый тон ВЫКЛ");
    }
#else
    (void)enable;
#endif
}

static bool ring_wait_command(uint32_t timeout_ms,
                              switchboard_ring_command_t *command)
{
    return (xQueueReceive(s_app.ring_queue, command,
                          pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}

static void ring_task(void *argument)
{
    (void)argument;
    switchboard_ring_command_t command;

    for (;;) {
        if (xQueueReceive(s_app.ring_queue, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (command == SWITCHBOARD_RING_STOP) {
            ring_audio_set(false);
            continue;
        }

        while (command == SWITCHBOARD_RING_START) {
            ring_audio_set(true);
            if (ring_wait_command(CONFIG_SWITCHBOARD_RING_ON_MS, &command)) {
                if (command == SWITCHBOARD_RING_STOP) {
                    break;
                }
            } else {
                ring_audio_set(false);
                if (!ring_wait_command(CONFIG_SWITCHBOARD_RING_OFF_MS, &command)) {
                    command = SWITCHBOARD_RING_START;
                }
            }
        }
        ring_audio_set(false);
    }
}

/* -------------------------------------------------------------------------- */
/* Потеря LU / аппаратная ошибка                                              */
/* -------------------------------------------------------------------------- */

static void notify_hardware_fault(int reason)
{
    const switchboard_control_event_t event = {
        .type = SWITCHBOARD_CONTROL_HARDWARE_FAULT,
        .reason = reason,
    };
    if (s_app.control_queue != NULL) {
        (void)xQueueSend(s_app.control_queue, &event, 0U);
    }
}

static void runtime_task(void *argument)
{
    switchboard_app_t *app = argument;
    uint32_t last_bsp_dropped = levisw_bsp_lu_event_dropped_count();
    uint32_t last_decoder_dropped = 0U;
    uint32_t last_read_errors = 0U;

    for (;;) {
        switchboard_control_event_t control;
        while (xQueueReceive(app->control_queue, &control, 0U) == pdTRUE) {
            if (control.type == SWITCHBOARD_CONTROL_HARDWARE_FAULT) {
                ESP_LOGE(TAG, "Аппаратная ошибка: reason=%d", control.reason);
                (void)call_runtime_notify_hardware_fault(&app->runtime,
                                                         esp_timer_get_time());
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
                const call_runtime_status_t feed =
                    call_runtime_feed_lu_edge(&app->runtime, &edge);
                if ((feed != CALL_RUNTIME_OK) &&
                    (feed != CALL_RUNTIME_ERR_INVALID_ARG)) {
                    ESP_LOGW(TAG, "Runtime отклонил LU edge: %d", (int)feed);
                }
                err = lu_event_dispatcher_decoder_receive(app->dispatcher,
                                                          &event, 0U);
            } while (err == ESP_OK);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Ошибка чтения очереди LU: %s", esp_err_to_name(err));
        }

        lu_event_dispatcher_stats_t stats;
        lu_event_dispatcher_get_stats(app->dispatcher, &stats);
        const uint32_t bsp_dropped = levisw_bsp_lu_event_dropped_count();

        if ((bsp_dropped != last_bsp_dropped) ||
            (stats.decoder_dropped != last_decoder_dropped) ||
            (stats.bsp_read_errors != last_read_errors)) {
            const bool levels[LU_DECODER_LINE_COUNT] = {
                levisw_bsp_lu1_read(),
                levisw_bsp_lu2_read(),
            };
            ESP_LOGE(TAG,
                     "Потеря LU: BSP=%" PRIu32 ", decoder=%" PRIu32
                     ", read_errors=%" PRIu32,
                     bsp_dropped - last_bsp_dropped,
                     stats.decoder_dropped - last_decoder_dropped,
                     stats.bsp_read_errors - last_read_errors);
            (void)call_runtime_notify_event_loss(&app->runtime,
                                                 levels,
                                                 esp_timer_get_time());
            last_bsp_dropped = bsp_dropped;
            last_decoder_dropped = stats.decoder_dropped;
            last_read_errors = stats.bsp_read_errors;
        }

        const call_runtime_status_t tick =
            call_runtime_tick(&app->runtime, esp_timer_get_time());
        if (tick == CALL_RUNTIME_ERR_FAULT) {
            ESP_LOGE(TAG, "Call Runtime в FAULT");
        } else if (tick != CALL_RUNTIME_OK) {
            ESP_LOGW(TAG, "Call Runtime tick=%d", (int)tick);
        }
    }
}

static void input_monitor_task(void *argument)
{
    switchboard_app_t *app = argument;
    bool alarm_level = false;
    bool alarm_level_known = false;
    bool alarm_fault_sent = false;
    int64_t next_alarm_poll_us = 0;

    for (;;) {
        levisw_bsp_lu_event_t event;
        const esp_err_t err = lu_event_dispatcher_monitor_receive(app->dispatcher,
                                                                  &event, 100U);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "INPUT LU%u level=%u t=%" PRId64 " us",
                     (unsigned)event.line,
                     event.level ? 1U : 0U,
                     event.timestamp_us);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Monitor queue error: %s", esp_err_to_name(err));
        }

#if CONFIG_SWITCHBOARD_ALARM_MONITOR_ENABLE
        const int64_t now = esp_timer_get_time();
        if (now >= next_alarm_poll_us) {
            bool current = false;
            const esp_err_t alarm_err = levisw_bsp_amp_alarm(&current);
            if (alarm_err == ESP_OK) {
                if (!alarm_level_known || (current != alarm_level)) {
                    ESP_LOGI(TAG, "INPUT ALARM-IN=%u", current ? 1U : 0U);
                    alarm_level = current;
                    alarm_level_known = true;
                }
#if CONFIG_SWITCHBOARD_ALARM_FAULT_ENABLE
                const bool active = CONFIG_SWITCHBOARD_ALARM_ACTIVE_HIGH
                                  ? current : !current;
                if (active && !alarm_fault_sent) {
                    ESP_LOGE(TAG, "ALARM-IN активен — запрос аппаратного FAULT");
                    notify_hardware_fault(0xA1);
                    alarm_fault_sent = true;
                } else if (!active) {
                    alarm_fault_sent = false;
                }
#endif
            } else {
                ESP_LOGW(TAG, "Не удалось прочитать ALARM-IN: %s",
                         esp_err_to_name(alarm_err));
            }
            next_alarm_poll_us = now + 250000;
        }
#endif
    }
}

#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
static void supervisor_task(void *argument)
{
    switchboard_app_t *app = argument;
    bool fault_sent = false;

    for (;;) {
        if (app->physical_matrix_enabled && !fault_sent) {
            const esp_err_t err = check_power_profile();
            if (err != ESP_OK) {
                notify_hardware_fault((int)err);
                fault_sent = true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500U));
    }
}
#else
static void supervisor_task(void *argument)
{
    (void)argument;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}
#endif

esp_err_t switchboard_app_start(void)
{
    memset(&s_app, 0, sizeof(s_app));
    memset(&s_simulated_matrix, 0, sizeof(s_simulated_matrix));
    memset(&s_ring_audio, 0, sizeof(s_ring_audio));

    ESP_LOGI(TAG,
             "Архитектура: D0..D7=ADG1608BR, E0..E9=TLE94112EL, MC33879APEKR2=OFF");

    esp_err_t err = levisw_bsp_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BSP init: %s", esp_err_to_name(err));
        return err;
    }

    s_app.control_queue = xQueueCreate(8U, sizeof(switchboard_control_event_t));
    s_app.ring_queue = xQueueCreate(8U, sizeof(switchboard_ring_command_t));
    if ((s_app.control_queue == NULL) || (s_app.ring_queue == NULL)) {
        return ESP_ERR_NO_MEM;
    }

#if CONFIG_SWITCHBOARD_ENABLE_PHYSICAL_MATRIX
    ESP_LOGW(TAG, "Физическая матрица разрешена конфигурацией; начинаю проверку профиля");
    err = init_physical_matrix();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Физическая матрица не запущена; все выходы переводятся в безопасное состояние");
        physical_best_effort_shutdown();
        return err;
    }
#else
    err = init_simulated_matrix();
    if (err != ESP_OK) {
        return err;
    }
#endif

    err = ring_audio_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DAC ring init failed: %s", esp_err_to_name(err));
        return err;
    }

    call_runtime_config_t runtime_cfg;
    call_runtime_default_config(&runtime_cfg);

    const bool initial_levels[LU_DECODER_LINE_COUNT] = {
        levisw_bsp_lu1_read(),
        levisw_bsp_lu2_read(),
    };
    const call_runtime_status_t runtime_status =
        call_runtime_init(&s_app.runtime,
                          &s_app.matrix,
                          &runtime_cfg,
                          initial_levels,
                          esp_timer_get_time(),
                          call_state_changed,
                          &s_app);
    if (runtime_status != CALL_RUNTIME_OK) {
        ESP_LOGE(TAG, "Runtime init failed: %d", (int)runtime_status);
        return ESP_FAIL;
    }

    lu_event_dispatcher_config_t dispatcher_cfg;
    lu_event_dispatcher_default_config(&dispatcher_cfg);
    err = lu_event_dispatcher_start(&dispatcher_cfg, &s_app.dispatcher);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LU dispatcher init: %s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(ring_task, "switchboard_ring", 4096U,
                    &s_app, 4U, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(runtime_task, "switchboard_runtime", 6144U,
                    &s_app, 6U, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(input_monitor_task, "switchboard_inputs", 4096U,
                    &s_app, 3U, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Не удалось создать задачу монитора входов");
    }
    if (xTaskCreate(supervisor_task, "switchboard_supervisor", 4096U,
                    &s_app, 5U, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Не удалось создать супервизор питания");
    }

    s_app.initialized = true;
    ESP_LOGI(TAG, "Контроллер готов; physical_matrix=%s, ring_dac=%s",
             s_app.physical_matrix_enabled ? "ON" : "SIMULATION",
#if CONFIG_SWITCHBOARD_ENABLE_DAC_RING
             "ON"
#else
             "OFF"
#endif
    );
    return ESP_OK;
}
