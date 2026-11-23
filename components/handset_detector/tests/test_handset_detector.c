#include "handset_detector.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int make_detector(handset_detector_t *detector)
{
    handset_detector_config_t config;
    handset_detector_default_config(&config);
    config.pickup_confirm_ms = 60U;
    config.hangup_confirm_ms = 80U;
    return handset_detector_init(detector, &config) ? 0 : 1;
}

static int test_pickup_requires_new_off_hook_edge_and_confirmation(void)
{
    handset_detector_t detector;
    CHECK(make_detector(&detector) == 0);
    CHECK(handset_detector_arm(&detector, HANDSET_LINE_1, true, 100000U));

    /* Already HIGH when armed is baseline, not a pickup event. */
    CHECK(handset_detector_poll(&detector, 100000U) == HANDSET_EVENT_NONE);
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, false, 110000U) ==
          HANDSET_EVENT_NONE);
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, true, 120500U) ==
          HANDSET_EVENT_PICKUP_CANDIDATE);
    CHECK(handset_detector_poll(&detector, 180499U) == HANDSET_EVENT_NONE);
    CHECK(handset_detector_poll(&detector, 180500U) == HANDSET_EVENT_PICKUP_CONFIRMED);
    CHECK(handset_detector_get_state(&detector) == HANDSET_STATE_TALKING);
    return 0;
}

static int test_short_pickup_glitch_is_rejected(void)
{
    handset_detector_t detector;
    CHECK(make_detector(&detector) == 0);
    CHECK(handset_detector_arm(&detector, HANDSET_LINE_2, false, 0U));
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_2, true, 10000U) ==
          HANDSET_EVENT_PICKUP_CANDIDATE);
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_2, false, 30000U) ==
          HANDSET_EVENT_PICKUP_REJECTED);
    CHECK(handset_detector_poll(&detector, 200000U) == HANDSET_EVENT_NONE);
    CHECK(handset_detector_get_state(&detector) == HANDSET_STATE_ARMED);
    return 0;
}

static int test_hangup_requires_sustained_on_hook_level(void)
{
    handset_detector_t detector;
    CHECK(make_detector(&detector) == 0);
    CHECK(handset_detector_arm(&detector, HANDSET_LINE_1, false, 0U));
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, true, 10000U) ==
          HANDSET_EVENT_PICKUP_CANDIDATE);
    CHECK(handset_detector_poll(&detector, 70000U) == HANDSET_EVENT_PICKUP_CONFIRMED);

    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, false, 100000U) ==
          HANDSET_EVENT_NONE);
    CHECK(handset_detector_get_state(&detector) == HANDSET_STATE_HANGUP_PENDING);
    CHECK(handset_detector_poll(&detector, 179999U) == HANDSET_EVENT_NONE);
    CHECK(handset_detector_poll(&detector, 180000U) == HANDSET_EVENT_HANGUP_CONFIRMED);
    CHECK(handset_detector_get_state(&detector) == HANDSET_STATE_DISARMED);
    return 0;
}

static int test_hangup_glitch_returns_to_talking(void)
{
    handset_detector_t detector;
    CHECK(make_detector(&detector) == 0);
    CHECK(handset_detector_arm(&detector, HANDSET_LINE_1, false, 0U));
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, true, 10000U) ==
          HANDSET_EVENT_PICKUP_CANDIDATE);
    CHECK(handset_detector_poll(&detector, 70000U) == HANDSET_EVENT_PICKUP_CONFIRMED);
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, false, 100000U) ==
          HANDSET_EVENT_NONE);
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, true, 120000U) ==
          HANDSET_EVENT_NONE);
    CHECK(handset_detector_get_state(&detector) == HANDSET_STATE_TALKING);
    CHECK(handset_detector_poll(&detector, 1000000U) == HANDSET_EVENT_NONE);
    return 0;
}

static int test_wrong_line_and_old_timestamps_are_ignored(void)
{
    handset_detector_t detector;
    CHECK(make_detector(&detector) == 0);
    CHECK(handset_detector_arm(&detector, HANDSET_LINE_1, false, 100U));
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_2, true, 110U) ==
          HANDSET_EVENT_NONE);
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, true, 90000U) ==
          HANDSET_EVENT_NONE);
    CHECK(handset_detector_get_state(&detector) == HANDSET_STATE_ARMED);
    return 0;
}


static int test_short_glitch_within_one_millisecond_is_not_rounded_away(void)
{
    handset_detector_t detector;
    CHECK(make_detector(&detector) == 0);
    CHECK(handset_detector_arm(&detector, HANDSET_LINE_1, false, 0U));
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, true, 100U) ==
          HANDSET_EVENT_PICKUP_CANDIDATE);
    CHECK(handset_detector_feed(&detector, HANDSET_LINE_1, false, 700U) ==
          HANDSET_EVENT_PICKUP_REJECTED);
    CHECK(handset_detector_poll(&detector, 100000U) == HANDSET_EVENT_NONE);
    CHECK(handset_detector_get_state(&detector) == HANDSET_STATE_ARMED);
    return 0;
}

int main(void)
{
    CHECK(test_pickup_requires_new_off_hook_edge_and_confirmation() == 0);
    CHECK(test_short_pickup_glitch_is_rejected() == 0);
    CHECK(test_hangup_requires_sustained_on_hook_level() == 0);
    CHECK(test_hangup_glitch_returns_to_talking() == 0);
    CHECK(test_wrong_line_and_old_timestamps_are_ignored() == 0);
    CHECK(test_short_glitch_within_one_millisecond_is_not_rounded_away() == 0);
    puts("All handset detector tests passed.");
    return 0;
}
