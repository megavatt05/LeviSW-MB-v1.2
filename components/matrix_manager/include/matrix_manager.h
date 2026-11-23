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

/* Physical crosspoint coordinate for one apartment: D0..D7 and E0..E9. */
typedef struct {
    uint8_t decade;
    uint8_t unit;
} matrix_manager_coordinate_t;

/*
 * Board-specific apartment map.
 *
 * verified is a commissioning approval flag, not an automatic hardware
 * measurement. Set it true only after all apartment contacts have been
 * traced against the current board/harness revision and independently checked.
 */
typedef struct {
    uint8_t version;
    bool verified;
    matrix_manager_coordinate_t apartments[MATRIX_MANAGER_FLAT_COUNT];
} matrix_manager_address_map_t;

/*
 * Build the conventional sequential numbering as an UNVERIFIED candidate.
 * It is useful for unit tests and simulation; it must not be enabled on a
 * physical panel without a completed continuity/commissioning record.
 */
void matrix_manager_address_map_make_sequential_candidate(
    matrix_manager_address_map_t *map);

/* Validate format, D/E ranges, and one-to-one use of all 80 crosspoints. */
bool matrix_manager_address_map_validate(const matrix_manager_address_map_t *map);

/*
 * Hardware adapter contract:
 * - set_mux(decade, false) selects the address while keeping ADG1608 disconnected.
 * - set_d_mask() drives MC33879A OUT1..OUT8; bit n selects Dn.
 * - set_all_units(FLOATING) leaves every TLE94112 output high impedance.
 * - set_unit() addresses E0..E9 (0-based); adapter performs the non-linear HB map.
 * - set_unit() and set_d_mask() must report SPI/diagnostic failures, not just enqueue.
 * - delay_us() must be a monotonic blocking delay suitable for the adapter's context.
 */
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
    uint32_t break_before_make_us; /* Provisional initial value; scope-verify. */
    uint32_t driver_wakeup_us;      /* Board report uses 15 ms after EN-DE rises. */
    uint32_t route_settle_us;       /* Provisional initial value; scope-verify. */
    matrix_unit_drive_t active_unit_drive; /* Must be chosen explicitly by the board profile. */
} matrix_manager_config_t;

typedef struct {
    matrix_manager_hw_ops_t hw;
    matrix_manager_config_t config;
    matrix_manager_address_map_t address_map;
    matrix_manager_state_t state;
    uint8_t active_apartment; /* 0 means no active route; otherwise 1..80. */
    matrix_manager_status_t last_fault;
    bool address_map_loaded;
    bool initialized;
} matrix_manager_t;

void matrix_manager_default_config(matrix_manager_config_t *config);

/* Initializes the manager and attempts to leave every matrix output safely open. */
matrix_manager_status_t matrix_manager_init(matrix_manager_t *manager,
                                             const matrix_manager_hw_ops_t *hw,
                                             const matrix_manager_config_t *config);

/*
 * Install a validated board map while the matrix is OPEN.
 * Unverified maps may be installed for inspection but cannot be routed.
 */
matrix_manager_status_t matrix_manager_set_address_map(
    matrix_manager_t *manager,
    const matrix_manager_address_map_t *map);

/* Route one apartment (1..80). Requires an installed map with verified=true. */
matrix_manager_status_t matrix_manager_connect(matrix_manager_t *manager,
                                                uint8_t apartment);

/* Open the crosspoint and leave all D/E outputs disabled. */
matrix_manager_status_t matrix_manager_disconnect(matrix_manager_t *manager);

/* Best-effort immediate safe-off; latches FAULT even if all off commands succeed. */
matrix_manager_status_t matrix_manager_emergency_stop(matrix_manager_t *manager);

/* Clear a latched fault only after all safe-off commands succeed. */
matrix_manager_status_t matrix_manager_recover(matrix_manager_t *manager);

matrix_manager_state_t matrix_manager_get_state(const matrix_manager_t *manager);
uint8_t matrix_manager_get_active_apartment(const matrix_manager_t *manager);
bool matrix_manager_is_ready(const matrix_manager_t *manager);

#ifdef __cplusplus
}
#endif

#endif /* MATRIX_MANAGER_H */
