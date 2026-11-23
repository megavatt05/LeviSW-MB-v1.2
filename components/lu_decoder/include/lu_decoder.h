#ifndef LU_DECODER_H
#define LU_DECODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LU_DECODER_LINE_COUNT       2U
#define LU_DECODER_RESULT_CAPACITY  8U

typedef enum {
    LU_DECODER_LINE_1 = 0,
    LU_DECODER_LINE_2 = 1,
} lu_decoder_line_t;

typedef struct {
    lu_decoder_line_t line;
    bool level;
    int64_t timestamp_us;
} lu_decoder_edge_t;

typedef struct {
    uint32_t glitch_filter_us;
    uint32_t burst_gap_us;
    uint8_t edges_per_address;
    uint16_t min_address;
    uint16_t max_address;
} lu_decoder_config_t;

typedef struct {
    lu_decoder_line_t line;
    uint16_t edge_count;
    uint16_t apartment;
    bool valid;
    int64_t completed_at_us;
} lu_decoder_result_t;

typedef struct {
    bool initialized;
    bool stable_level;
    bool pending;
    bool pending_level;
    int64_t pending_since_us;
    int64_t last_input_timestamp_us;
    bool burst_active;
    bool burst_malformed;
    bool desynchronized;
    uint16_t burst_edge_count;
    int64_t last_valid_edge_us;
} lu_decoder_line_state_t;

typedef struct {
    lu_decoder_config_t config;
    lu_decoder_line_state_t lines[LU_DECODER_LINE_COUNT];
    lu_decoder_result_t results[LU_DECODER_RESULT_CAPACITY];
    size_t result_head;
    size_t result_count;
    uint32_t invalid_timestamps;
    uint32_t dropped_results;
} lu_decoder_t;

/*
 * Исторические стендовые заметки указывают на фильтрацию помех длительностью 120 мкс, завершение пакета через 80 мс
 * без подтверждённого фронта и два принятых перехода на номер квартиры. Эти значения
 * являются только начальными параметрами для автономной проверки; подтвердите их на реальной панели
 * осциллографом или логическим анализатором до подключения линий домофона.
 */
void lu_decoder_default_config(lu_decoder_config_t *config);
bool lu_decoder_init(lu_decoder_t *decoder, const lu_decoder_config_t *config);

/* Задать исходный уровень покоя до обработки событий. События с меткой времени не позже этого момента считаются устаревшими. */
bool lu_decoder_set_line_level(lu_decoder_t *decoder,
                               lu_decoder_line_t line,
                               bool level,
                               int64_t timestamp_us);

/* Передать необработанные фронты, не зависящие от GPIO; фильтрация и учёт пакетов выполняются внутри декодера. */
void lu_decoder_feed(lu_decoder_t *decoder, const lu_decoder_edge_t *edge);

/* Продвинуть внутреннее время при отсутствии новых фронтов; это может завершить один или оба пакета. */
void lu_decoder_poll(lu_decoder_t *decoder, int64_t now_us);

/* Извлечь завершённые или некорректные пакеты в порядке FIFO. */
bool lu_decoder_result_pop(lu_decoder_t *decoder, lu_decoder_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* Защита от повторного включения заголовочного файла. */
