#include "handset_detector.h"

#include <string.h>

static bool valid_line(handset_line_t line)
{
    return (line == HANDSET_LINE_1) || (line == HANDSET_LINE_2);
}

void handset_detector_default_config(handset_detector_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->off_hook_level = true;
    config->pickup_confirm_ms = 60U;
    config->hangup_confirm_ms = 60U;
}

bool handset_detector_init(handset_detector_t *detector,
                           const handset_detector_config_t *config)
{
    if ((detector == NULL) || (config == NULL) ||
        (config->pickup_confirm_ms == 0U) ||
        (config->hangup_confirm_ms == 0U)) {
        return false;
    }

    memset(detector, 0, sizeof(*detector));
    detector->config = *config;
    detector->state = HANDSET_STATE_DISARMED;
    detector->initialized = true;
    return true;
}

bool handset_detector_arm(handset_detector_t *detector,
                          handset_line_t line,
                          bool initial_level,
                          int64_t now_us)
{
    if ((detector == NULL) || !detector->initialized ||
        !valid_line(line) || (now_us < 0)) {
        return false;
    }

    detector->line = line;
    detector->stable_level = initial_level;
    detector->last_timestamp_us = now_us;
    detector->pending_since_us = 0;
    detector->state = HANDSET_STATE_ARMED;
    return true;
}

void handset_detector_disarm(handset_detector_t *detector)
{
    if ((detector == NULL) || !detector->initialized) {
        return;
    }
    detector->state = HANDSET_STATE_DISARMED;
    detector->pending_since_us = 0;
}

handset_event_t handset_detector_feed(handset_detector_t *detector,
                                       handset_line_t line,
                                       bool level,
                                       int64_t timestamp_us)
{
    if ((detector == NULL) || !detector->initialized ||
        (detector->state == HANDSET_STATE_DISARMED) ||
        (line != detector->line) || (timestamp_us < 0) ||
        (timestamp_us <= detector->last_timestamp_us)) {
        return HANDSET_EVENT_NONE;
    }

    detector->last_timestamp_us = timestamp_us;
    if (level == detector->stable_level) {
        return HANDSET_EVENT_NONE;
    }

    detector->stable_level = level;

    switch (detector->state) {
    case HANDSET_STATE_ARMED:
        if (level == detector->config.off_hook_level) {
            detector->pending_since_us = timestamp_us;
            detector->state = HANDSET_STATE_PICKUP_PENDING;
            return HANDSET_EVENT_PICKUP_CANDIDATE;
        }
        return HANDSET_EVENT_NONE;

    case HANDSET_STATE_PICKUP_PENDING:
        if (level != detector->config.off_hook_level) {
            detector->pending_since_us = 0;
            detector->state = HANDSET_STATE_ARMED;
            detector->rejected_pickups++;
            return HANDSET_EVENT_PICKUP_REJECTED;
        }
        return HANDSET_EVENT_NONE;

    case HANDSET_STATE_TALKING:
        if (level != detector->config.off_hook_level) {
            detector->pending_since_us = timestamp_us;
            detector->state = HANDSET_STATE_HANGUP_PENDING;
        }
        return HANDSET_EVENT_NONE;

    case HANDSET_STATE_HANGUP_PENDING:
        if (level == detector->config.off_hook_level) {
            detector->pending_since_us = 0;
            detector->state = HANDSET_STATE_TALKING;
        }
        return HANDSET_EVENT_NONE;

    case HANDSET_STATE_DISARMED:
    default:
        return HANDSET_EVENT_NONE;
    }
}

handset_event_t handset_detector_poll(handset_detector_t *detector,
                                       int64_t now_us)
{
    if ((detector == NULL) || !detector->initialized ||
        (detector->state == HANDSET_STATE_DISARMED) ||
        (now_us < detector->last_timestamp_us) ||
        (now_us < detector->pending_since_us)) {
        return HANDSET_EVENT_NONE;
    }

    detector->last_timestamp_us = now_us;
    const uint64_t elapsed_us = (uint64_t)(now_us - detector->pending_since_us);

    if ((detector->state == HANDSET_STATE_PICKUP_PENDING) &&
        (elapsed_us >= ((uint64_t)detector->config.pickup_confirm_ms * 1000ULL))) {
        detector->state = HANDSET_STATE_TALKING;
        detector->confirmed_pickups++;
        return HANDSET_EVENT_PICKUP_CONFIRMED;
    }

    if ((detector->state == HANDSET_STATE_HANGUP_PENDING) &&
        (elapsed_us >= ((uint64_t)detector->config.hangup_confirm_ms * 1000ULL))) {
        detector->state = HANDSET_STATE_DISARMED;
        detector->confirmed_hangups++;
        return HANDSET_EVENT_HANGUP_CONFIRMED;
    }

    return HANDSET_EVENT_NONE;
}

handset_state_t handset_detector_get_state(const handset_detector_t *detector)
{
    return (detector == NULL) ? HANDSET_STATE_DISARMED : detector->state;
}
