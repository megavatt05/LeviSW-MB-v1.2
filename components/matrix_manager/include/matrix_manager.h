#ifndef MATRIX_MANAGER_H
#define MATRIX_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MATRIX_MANAGER_FLAT_COUNT 80U
#define MATRIX_MANAGER_DOZEN_COUNT 8U
#define MATRIX_MANAGER_UNIT_COUNT 10U
#define MATRIX_MANAGER_ADDRESS_MAP_VERSION 1U

#define MATRIX_MANAGER_REVISION_TEXT_LEN 16U
#define MATRIX_MANAGER_RECORD_TEXT_LEN 48U

typedef enum {
    MATRIX_MANAGER_OK = 0,
    MATRIX_MANAGER_ERR_INVALID_ARG = -1,
    MATRIX_MANAGER_ERR_INVALID_STATE = -2,
    MATRIX_MANAGER_ERR_IO = -3,
    MATRIX_MANAGER_ERR_FAULT = -4,
    MATRIX_MANAGER_ERR_ADDRESS_MAP_UNVERIFIED = -5,
} matrix_manager_status_t;

typedef enum {
    MATRIX_UNIT_FLOATING = 0,
    MATRIX_UNIT_LOW_SIDE = 1,
    MATRIX_UNIT_HIGH_SIDE = 2,
} matrix_unit_drive_t;

typedef enum {
    MATRIX_MANAGER_STATE_UNINITIALIZED = 0,
    MATRIX_MANAGER_STATE_OPEN,
    MATRIX_MANAGER_STATE_CONNECTED,
    MATRIX_MANAGER_STATE_FAULT,
} matrix_manager_state_t;

typedef struct {
    uint8_t decade;
    uint8_t unit;
} matrix_manager_coordinate_t;

/*
 * Адресная карта квартир для конкретной платы.
 *
 * Флаг verified обозначает утверждение после ввода в эксплуатацию, а не автоматическое аппаратное
 * измерение. В утверждённой карте должны быть указаны ревизия печатной платы, ревизия жгута
 * и контролируемый протокол проверки. Протокол должен включать
 * результаты проверки целостности цепей и независимую проверку.
 */
typedef struct {
    uint8_t version;
    bool verified;
    char pcb_revision[MATRIX_MANAGER_REVISION_TEXT_LEN];
    char harness_revision[MATRIX_MANAGER_REVISION_TEXT_LEN];
    char commissioning_record[MATRIX_MANAGER_RECORD_TEXT_LEN];
    matrix_manager_coordinate_t apartments[MATRIX_MANAGER_FLAT_COUNT];
} matrix_manager_address_map_t;

/*
 * Сформировать стандартную последовательную нумерацию как НЕПРОВЕРЕННЫЙ шаблон.
 * Он пригоден для модульных тестов и симуляции; его нельзя использовать на
 * физической панели без заполненного протокола проверки целостности цепей и ввода в эксплуатацию.
 */
void matrix_manager_address_map_make_sequential_candidate(
    matrix_manager_address_map_t *map);

/* Проверить формат, идентификационные метаданные, диапазоны D/E и уникальность точек коммутации. */
bool matrix_manager_address_map_validate(const matrix_manager_address_map_t *map);

typedef struct {
    void *context;
    matrix_manager_status_t (*set_mux)(void *context, uint8_t decade, bool enable);
    matrix_manager_status_t (*set_d_mask)(void *context, uint8_t mask);
    matrix_manager_status_t (*set_all_units)(void *context, matrix_unit_drive_t drive);
    matrix_manager_status_t (*set_unit)(void *context, uint8_t unit, matrix_unit_drive_t drive);
    matrix_manager_status_t (*set_drivers_enabled)(void *context, bool enabled);
    void (*delay_us)(void *context, uint32_t delay_us);
} matrix_manager_hw_ops_t;

typedef struct {
    uint32_t break_before_make_us; /* Предварительное начальное значение; проверить осциллографом. */
    uint32_t driver_wakeup_us;      /* В отчёте по плате указана задержка 15 мс после перехода EN-DE в HIGH. */
    uint32_t route_settle_us;       /* Предварительное начальное значение; проверить осциллографом. */
    matrix_unit_drive_t active_unit_drive; /* Должно быть явно задано в аппаратном профиле платы. */
} matrix_manager_config_t;

typedef struct {
    matrix_manager_hw_ops_t hw;
    matrix_manager_config_t config;
    matrix_manager_address_map_t address_map;
    matrix_manager_state_t state;
    uint8_t active_apartment;
    matrix_manager_status_t last_fault;
    bool address_map_loaded;
    bool initialized;
} matrix_manager_t;

void matrix_manager_default_config(matrix_manager_config_t *config);

/* Инициализировать менеджер и попытаться безопасно разомкнуть все выходы матрицы. */
matrix_manager_status_t matrix_manager_init(matrix_manager_t *manager,
                                             const matrix_manager_hw_ops_t *hw,
                                             const matrix_manager_config_t *config);

/* Установить проверенную карту платы, пока матрица находится в OPEN. По непроверенной карте маршрутизация запрещена. */
matrix_manager_status_t matrix_manager_set_address_map(
    matrix_manager_t *manager,
    const matrix_manager_address_map_t *map);

/* Подключить маршрут одной квартиры (1..80). Требуется проверенная карта с метаданными прослеживаемости. */
matrix_manager_status_t matrix_manager_connect(matrix_manager_t *manager,
                                                uint8_t apartment);

matrix_manager_status_t matrix_manager_disconnect(matrix_manager_t *manager);
matrix_manager_status_t matrix_manager_emergency_stop(matrix_manager_t *manager);
matrix_manager_status_t matrix_manager_recover(matrix_manager_t *manager);

matrix_manager_state_t matrix_manager_get_state(const matrix_manager_t *manager);
uint8_t matrix_manager_get_active_apartment(const matrix_manager_t *manager);
/* Возвращает true, только если матрица разомкнута/подключена И загружена проверенная адресная карта. */
bool matrix_manager_is_ready(const matrix_manager_t *manager);

#ifdef __cplusplus
}
#endif

#endif /* Защита от повторного включения заголовочного файла. */
