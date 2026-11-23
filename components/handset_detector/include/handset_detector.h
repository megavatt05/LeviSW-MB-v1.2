#ifndef HANDSET_DETECTOR_H
#define HANDSET_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HANDSET_LINE_1 = 0,
    HANDSET_LINE_2 = 1,
} handset_line_t;

typedef enum {
    HANDSET_EVENT_NONE = 0,
    HANDSET_EVENT_PICKUP_CANDIDATE,
    HANDSET_EVENT_PICKUP_CONFIRMED,
    HANDSET_EVENT_PICKUP_REJECTED,
    HANDSET_EVENT_HANGUP_CONFIRMED,
} handset_event_t;

typedef enum {
    HANDSET_STATE_DISARMED = 0,
    HANDSET_STATE_ARMED,
    HANDSET_STATE_PICKUP_PENDING,
    HANDSET_STATE_TALKING,
    HANDSET_STATE_HANGUP_PENDING,
} handset_state_t;

typedef struct {
    bool off_hook_level;           /* Published LU example uses HIGH=Off-Hook; configurable. */
    uint32_t pickup_confirm_ms;    /* Provisional; calibrate against the actual intercom. */
    uint32_t hangup_confirm_ms;    /* Provisional; calibrate against the actual intercom. */
} handset_detector_config_t;

typedef struct {
    handset_detector_config_t config;
    handset_state_t state;
    handset_line_t line;
    bool stable_level;
    int64_t pending_since_us;
    int64_t last_timestamp_us;
    uint32_t rejected_pickups;
    uint32_t confirmed_pickups;
    uint32_t confirmed_hangups;
    bool initialized;
} handset_detector_t;

void handset_detector_default_config(handset_detector_config_t *config);
bool handset_detector_init(handset_detector_t *detector,
                           const handset_detector_config_t *config);

/* Arm only after a subscriber route is connected; current level becomes baseline. */
bool handset_detector_arm(handset_detector_t *detector,
                          handset_line_t line,
                          bool initial_level,
                          int64_t now_us);
void handset_detector_disarm(handset_detector_t *detector);

/* Feed timestamped LU level changes. Timestamps are monotonic microseconds. */
handset_event_t handset_detector_feed(handset_detector_t *detector,
                                       handset_line_t line,
                                       bool level,
                                       int64_t timestamp_us);

/* Advance pickup/hangup verification without requiring another edge. */
handset_event_t handset_detector_poll(handset_detector_t *detector,
                                       int64_t now_us);

handset_state_t handset_detector_get_state(const handset_detector_t *detector);

#ifdef __cplusplus
}
#endif

#endif /* HANDSET_DETECTOR_H */
