#include "lu_decoder.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static void emit_address(lu_decoder_t *decoder,
                         lu_decoder_line_t line,
                         uint16_t address,
                         bool *level,
                         int64_t *time_us,
                         uint16_t omit_edge_index)
{
    const uint16_t edge_total = (uint16_t)(address * 2U);
    for (uint16_t i = 0; i < edge_total; ++i) {
        if (i == omit_edge_index) {
            *level = !*level;
            *time_us += 1000;
            continue;
        }
        *level = !*level;
        *time_us += 1000;
        const lu_decoder_edge_t edge = {
            .line = line,
            .level = *level,
            .timestamp_us = *time_us,
        };
        lu_decoder_feed(decoder, &edge);
        lu_decoder_poll(decoder, *time_us + 200);
    }
}

static bool finish_and_get(lu_decoder_t *decoder,
                           int64_t now_us,
                           lu_decoder_result_t *result)
{
    lu_decoder_poll(decoder, now_us + 80001);
    return lu_decoder_result_pop(decoder, result);
}

static int test_all_apartment_addresses(void)
{
    for (uint16_t expected = 1U; expected <= 80U; ++expected) {
        lu_decoder_t decoder;
        lu_decoder_config_t config;
        lu_decoder_default_config(&config);
        CHECK(lu_decoder_init(&decoder, &config));
        CHECK(lu_decoder_set_line_level(&decoder, LU_DECODER_LINE_1, false, 0));

        bool level = false;
        int64_t time_us = 1000;
        emit_address(&decoder, LU_DECODER_LINE_1, expected, &level, &time_us, UINT16_MAX);

        lu_decoder_result_t result;
        CHECK(finish_and_get(&decoder, time_us + 200, &result));
        CHECK(result.valid);
        CHECK(result.apartment == expected);
        CHECK(result.edge_count == (uint16_t)(expected * 2U));
        CHECK(result.line == LU_DECODER_LINE_1);
        CHECK(!lu_decoder_result_pop(&decoder, &result));
    }
    return 0;
}

static int test_short_glitch_is_rejected(void)
{
    lu_decoder_t decoder;
    lu_decoder_config_t config;
    lu_decoder_default_config(&config);
    CHECK(lu_decoder_init(&decoder, &config));
    CHECK(lu_decoder_set_line_level(&decoder, LU_DECODER_LINE_1, false, 0));

    lu_decoder_edge_t edge = { LU_DECODER_LINE_1, true, 1000 };
    lu_decoder_feed(&decoder, &edge);
    edge.level = false;
    edge.timestamp_us = 1050;
    lu_decoder_feed(&decoder, &edge);

    bool level = false;
    int64_t time_us = 10000;
    emit_address(&decoder, LU_DECODER_LINE_1, 7U, &level, &time_us, UINT16_MAX);

    lu_decoder_result_t result;
    CHECK(finish_and_get(&decoder, time_us + 200, &result));
    CHECK(result.valid);
    CHECK(result.apartment == 7U);
    CHECK(result.edge_count == 14U);
    return 0;
}

static int test_missing_half_pulse_is_invalid(void)
{
    lu_decoder_t decoder;
    lu_decoder_config_t config;
    lu_decoder_default_config(&config);
    CHECK(lu_decoder_init(&decoder, &config));
    CHECK(lu_decoder_set_line_level(&decoder, LU_DECODER_LINE_1, false, 0));

    bool level = false;
    int64_t time_us = 1000;
    /* One lost transition causes a repeated sampled level and invalidates the burst. */
    emit_address(&decoder, LU_DECODER_LINE_1, 9U, &level, &time_us, 5U);

    lu_decoder_result_t result;
    CHECK(finish_and_get(&decoder, time_us + 200, &result));
    CHECK(!result.valid);
    CHECK(result.apartment == 0U);
    return 0;
}

static int test_complete_missing_pulse_limitation_is_explicit(void)
{
    lu_decoder_t decoder;
    lu_decoder_config_t config;
    lu_decoder_default_config(&config);
    CHECK(lu_decoder_init(&decoder, &config));
    CHECK(lu_decoder_set_line_level(&decoder, LU_DECODER_LINE_1, false, 0));

    bool level = false;
    int64_t time_us = 1000;
    /* Pulse-count-only framing cannot know that a whole pulse pair was omitted. */
    emit_address(&decoder, LU_DECODER_LINE_1, 8U, &level, &time_us, UINT16_MAX);

    lu_decoder_result_t result;
    CHECK(finish_and_get(&decoder, time_us + 200, &result));
    CHECK(result.valid);
    CHECK(result.apartment == 8U);
    return 0;
}

static int test_out_of_range_address_is_invalid(void)
{
    lu_decoder_t decoder;
    lu_decoder_config_t config;
    lu_decoder_default_config(&config);
    CHECK(lu_decoder_init(&decoder, &config));
    CHECK(lu_decoder_set_line_level(&decoder, LU_DECODER_LINE_2, true, 0));

    bool level = true;
    int64_t time_us = 1000;
    emit_address(&decoder, LU_DECODER_LINE_2, 81U, &level, &time_us, UINT16_MAX);

    lu_decoder_result_t result;
    CHECK(finish_and_get(&decoder, time_us + 200, &result));
    CHECK(!result.valid);
    CHECK(result.line == LU_DECODER_LINE_2);
    return 0;
}

int main(void)
{
    CHECK(test_all_apartment_addresses() == 0);
    CHECK(test_short_glitch_is_rejected() == 0);
    CHECK(test_missing_half_pulse_is_invalid() == 0);
    CHECK(test_complete_missing_pulse_limitation_is_explicit() == 0);
    CHECK(test_out_of_range_address_is_invalid() == 0);
    puts("All LU decoder tests passed.");
    return 0;
}
