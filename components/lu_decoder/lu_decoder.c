#include "lu_decoder.h"

#include <string.h>

static bool line_is_valid(lu_decoder_line_t line)
{
    return (line == LU_DECODER_LINE_1) || (line == LU_DECODER_LINE_2);
}

void lu_decoder_default_config(lu_decoder_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->glitch_filter_us = 120U;
    config->burst_gap_us = 80000U;
    config->edges_per_address = 2U;
    config->min_address = 1U;
    config->max_address = 80U;
}

bool lu_decoder_init(lu_decoder_t *decoder, const lu_decoder_config_t *config)
{
    if ((decoder == NULL) || (config == NULL) ||
        (config->glitch_filter_us == 0U) ||
        (config->burst_gap_us <= config->glitch_filter_us) ||
        (config->edges_per_address == 0U) ||
        (config->min_address == 0U) ||
        (config->min_address > config->max_address)) {
        return false;
    }

    memset(decoder, 0, sizeof(*decoder));
    decoder->config = *config;
    return true;
}

bool lu_decoder_set_line_level(lu_decoder_t *decoder,
                               lu_decoder_line_t line,
                               bool level,
                               int64_t timestamp_us)
{
    if ((decoder == NULL) || !line_is_valid(line) || (timestamp_us < 0)) {
        return false;
    }

    lu_decoder_line_state_t *state = &decoder->lines[(size_t)line];
    memset(state, 0, sizeof(*state));
    state->initialized = true;
    state->stable_level = level;
    state->last_input_timestamp_us = timestamp_us;
    return true;
}

static void queue_result(lu_decoder_t *decoder,
                         lu_decoder_line_t line,
                         lu_decoder_line_state_t *state,
                         int64_t completed_at_us)
{
    if (decoder->result_count >= LU_DECODER_RESULT_CAPACITY) {
        decoder->dropped_results++;
    } else {
        const size_t tail = (decoder->result_head + decoder->result_count) %
                            LU_DECODER_RESULT_CAPACITY;
        lu_decoder_result_t *result = &decoder->results[tail];
        const uint16_t divisor = decoder->config.edges_per_address;
        const bool divisible = (state->burst_edge_count % divisor) == 0U;
        const uint16_t address = divisible
                               ? (uint16_t)(state->burst_edge_count / divisor)
                               : 0U;
        const bool valid = divisible && !state->burst_malformed &&
                           (address >= decoder->config.min_address) &&
                           (address <= decoder->config.max_address);

        result->line = line;
        result->edge_count = state->burst_edge_count;
        result->apartment = valid ? address : 0U;
        result->valid = valid;
        result->completed_at_us = completed_at_us;
        decoder->result_count++;
    }

    state->burst_active = false;
    state->burst_malformed = false;
    state->burst_edge_count = 0U;
    state->last_valid_edge_us = 0;
}

void lu_decoder_poll(lu_decoder_t *decoder, int64_t now_us)
{
    if ((decoder == NULL) || (now_us < 0)) {
        return;
    }

    for (size_t i = 0; i < LU_DECODER_LINE_COUNT; ++i) {
        lu_decoder_line_state_t *state = &decoder->lines[i];

        if (!state->initialized) {
            continue;
        }

        if (state->pending && (now_us >= state->pending_since_us) &&
            ((uint64_t)(now_us - state->pending_since_us) >= decoder->config.glitch_filter_us)) {
            state->stable_level = state->pending_level;
            state->pending = false;

            if (!state->burst_active) {
                state->burst_active = true;
                state->burst_malformed = false;
                state->burst_edge_count = 1U;
            } else if (state->burst_edge_count < UINT16_MAX) {
                state->burst_edge_count++;
            }
            state->last_valid_edge_us = state->pending_since_us;
        }

        /* Do not terminate a burst while a possible edge is still being qualified. */
        if (state->burst_active && !state->pending &&
            (now_us >= state->last_valid_edge_us) &&
            ((uint64_t)(now_us - state->last_valid_edge_us) >= decoder->config.burst_gap_us)) {
            queue_result(decoder, (lu_decoder_line_t)i, state, now_us);
        }
    }
}

void lu_decoder_feed(lu_decoder_t *decoder, const lu_decoder_edge_t *edge)
{
    if ((decoder == NULL) || (edge == NULL) || !line_is_valid(edge->line) ||
        (edge->timestamp_us < 0)) {
        return;
    }

    lu_decoder_line_state_t *state = &decoder->lines[(size_t)edge->line];
    if (!state->initialized) {
        (void)lu_decoder_set_line_level(decoder, edge->line,
                                        edge->level, edge->timestamp_us);
        return;
    }

    if (edge->timestamp_us <= state->last_input_timestamp_us) {
        decoder->invalid_timestamps++;
        return;
    }

    /* First close any burst whose quiet gap ended before this new edge. */
    lu_decoder_poll(decoder, edge->timestamp_us);
    state->last_input_timestamp_us = edge->timestamp_us;

    if (state->pending) {
        if (edge->level == state->pending_level) {
            return; /* duplicate sample of the candidate level */
        }
        if (edge->level == state->stable_level) {
            /* A return to the old state before the filter expires is a short glitch. */
            if ((uint64_t)(edge->timestamp_us - state->pending_since_us) <
                decoder->config.glitch_filter_us) {
                state->pending = false;
                return;
            }
            /* Otherwise poll() above has already committed the previous candidate. */
        }
    }

    if (!state->pending && (edge->level == state->stable_level) && state->burst_active) {
        /* A repeated sampled level inside a burst means at least one transition was lost. */
        state->burst_malformed = true;
    }

    if (!state->pending && (edge->level != state->stable_level)) {
        state->pending = true;
        state->pending_level = edge->level;
        state->pending_since_us = edge->timestamp_us;
    }
}

bool lu_decoder_result_pop(lu_decoder_t *decoder, lu_decoder_result_t *result)
{
    if ((decoder == NULL) || (result == NULL) || (decoder->result_count == 0U)) {
        return false;
    }

    *result = decoder->results[decoder->result_head];
    decoder->result_head = (decoder->result_head + 1U) % LU_DECODER_RESULT_CAPACITY;
    decoder->result_count--;
    return true;
}
