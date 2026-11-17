#include <string.h>
#include "mc33879a.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "MC33879A";

esp_err_t mc33879a_init(const mc33879a_config_t *config, mc33879a_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(config && out_handle, ESP_ERR_INVALID_ARG, TAG, "Некорректные аргументы");
    memset(out_handle, 0, sizeof(mc33879a_handle_t));
    out_handle->en_io_num = config->en_io_num;

    /* 1. Настройка пина EN, если он подключен к GPIO */
    if (config->en_io_num >= 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << config->en_io_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Не удалось настроить пин EN");
        /* Переводим микросхему в активный режим (EN = 1) */
        ESP_RETURN_ON_ERROR(gpio_set_level(config->en_io_num, 1), TAG, "Не удалось установить EN=1");
    }

    /* 2. Подключение уже созданного SPI-устройства (из BSP) или добавление нового */
    if (config->existing_spi_dev != NULL) {
        out_handle->spi_dev = config->existing_spi_dev;
        out_handle->owns_spi_dev = false;
    } else {
        if (config->init_spi_bus) {
            spi_bus_config_t buscfg = {
                .mosi_io_num = config->mosi_io_num,
                .miso_io_num = config->miso_io_num,
                .sclk_io_num = config->sclk_io_num,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = 4, /* Передаем короткие 16-битные пакеты без DMA */
            };
            ESP_RETURN_ON_ERROR(spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_DISABLED),
                                TAG, "Ошибка инициализации шины SPI");
        }

        /* КРИТИЧНО: MC33879A работает строго в SPI Mode 1 (CPOL = 0, CPHA = 1) */
        spi_device_interface_config_t devcfg = {
            .clock_speed_hz = (config->clock_speed_hz > 0) ? config->clock_speed_hz : (1 * 1000 * 1000),
            .mode = 1,                          /* CPOL=0, CPHA=1 (захват данных по спаду SCLK) */
            .spics_io_num = config->cs_io_num,
            .queue_size = 2,
            .cs_ena_pretrans = 2,               /* Соблюдение t_LEAD */
            .cs_ena_posttrans = 2,              /* Соблюдение t_LAG */
        };

        ESP_RETURN_ON_ERROR(spi_bus_add_device(config->spi_host, &devcfg, &out_handle->spi_dev),
                            TAG, "Ошибка добавления устройства MC33879A на шину SPI");
        out_handle->owns_spi_dev = true;
    }

    /* Сбрасываем все выходы в 0 при старте и читаем начальный статус */
    uint8_t initial_faults = 0;
    esp_err_t ret = mc33879a_set_outputs(out_handle, 0x00, &initial_faults);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MC33879A инициализирован (EN=%d). Начальная маска диагностики: 0x%02X",
                 out_handle->en_io_num, initial_faults);
    }
    return ret;
}

esp_err_t mc33879a_deinit(mc33879a_handle_t *handle, bool free_spi_bus, spi_host_device_t spi_host)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Дескриптор NULL");

    if (handle->spi_dev) {
        (void)mc33879a_set_outputs(handle, 0x00, NULL);
        if (handle->owns_spi_dev) {
            spi_bus_remove_device(handle->spi_dev);
        }
        handle->spi_dev = NULL;
    }

    if (handle->en_io_num >= 0) {
        gpio_set_level(handle->en_io_num, 0);
    }

    if (free_spi_bus) {
        return spi_bus_free(spi_host);
    }
    return ESP_OK;
}

esp_err_t mc33879a_set_enable(mc33879a_handle_t *handle, bool enable)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Дескриптор NULL");
    ESP_RETURN_ON_FALSE(handle->en_io_num >= 0, ESP_ERR_INVALID_STATE, TAG, "Пин EN не настроен");

    return gpio_set_level(handle->en_io_num, enable ? 1 : 0);
}

esp_err_t mc33879a_transfer_raw(mc33879a_handle_t *handle, uint16_t tx_word, uint16_t *rx_word)
{
    ESP_RETURN_ON_FALSE(handle && handle->spi_dev, ESP_ERR_INVALID_ARG, TAG, "Драйвер не инициализирован");

    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,   /* Ровно 16 тактов SCLK */
        .rxlength = 16,
    };

    /* Явная укладка в Big-Endian (MSB first):
     * tx_data[0] уходит по линии MOSI первым (биты D15..D8)
     * tx_data[1] уходит по линии MOSI вторым (биты D7..D0) */
    t.tx_data[0] = (uint8_t)((tx_word >> 8) & 0xFFU);
    t.tx_data[1] = (uint8_t)(tx_word & 0xFFU);

    esp_err_t ret = spi_device_transmit(handle->spi_dev, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка обмена SPI: %s", esp_err_to_name(ret));
        return ret;
    }

    if (rx_word != NULL) {
        *rx_word = ((uint16_t)t.rx_data[0] << 8) | (uint16_t)t.rx_data[1];
    }

    return ESP_OK;
}

esp_err_t mc33879a_set_outputs(mc33879a_handle_t *handle, uint8_t out_mask, uint8_t *fault_mask)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Дескриптор NULL");

    /* Формат 16-битного слова MC33879A:
     * D15..D8 = 0x00 (прямое управление без ШИМ),
     * D7..D0  = ON/OFF состояние OUT8..OUT1. */
    uint16_t tx_word = (uint16_t)out_mask;
    uint16_t rx_word = 0;

    esp_err_t ret = mc33879a_transfer_raw(handle, tx_word, &rx_word);
    if (ret != ESP_OK) {
        return ret;
    }

    handle->current_out_mask = out_mask;

    /* В ответном слове DO [7:0] содержатся биты статуса/ошибок каналов 8..1 предыдущей команды */
    uint8_t faults = (uint8_t)(rx_word & 0xFFU);
    handle->last_fault_mask = faults;

    if (fault_mask != NULL) {
        *fault_mask = faults;
    }

    ESP_LOGD(TAG, "TX: 0x%04X | RX: 0x%04X (OUT=0x%02X, FAULT=0x%02X)",
             tx_word, rx_word, out_mask, faults);

    return ESP_OK;
}

esp_err_t mc33879a_set_channel(mc33879a_handle_t *handle, uint8_t channel, bool state, uint8_t *fault_mask)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Дескриптор NULL");
    ESP_RETURN_ON_FALSE(channel >= 1 && channel <= 8, ESP_ERR_INVALID_ARG, TAG, "Канал должен быть 1..8");

    uint8_t new_mask = handle->current_out_mask;
    uint8_t bit = (uint8_t)(1U << (channel - 1));

    if (state) {
        new_mask |= bit;
    } else {
        new_mask &= (uint8_t)(~bit);
    }

    return mc33879a_set_outputs(handle, new_mask, fault_mask);
}

esp_err_t mc33879a_read_faults(mc33879a_handle_t *handle, uint8_t *fault_mask)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Дескриптор NULL");
    return mc33879a_set_outputs(handle, handle->current_out_mask, fault_mask);
}
