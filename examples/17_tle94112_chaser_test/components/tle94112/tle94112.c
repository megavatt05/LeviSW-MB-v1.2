/**
 * @file tle94112.c
 * @brief ESP-IDF Driver for Infineon TLE94112EL Motor Driver
 *
 * Изменения от 2026-03-10:
 * - Добавлены функции программной защиты (overcurrent/overvoltage)
 * - Добавлена структура конфигурации защит
 * - Улучшена обработка ошибок SPI
 * - Добавлена функция аварийного отключения всех выходов
 *
 * Исправлены:
 * - Формирование адреса (бит LABT=1)
 * - Проверка Device ID
 * - Карта регистров
 */

#include "tle94112.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "TLE94112";

// === Регистры TLE94112 (MSB-first) ===
// Формат кадра SPI (16 бит): [Байт Адреса] [Байт Данных]
// Байт адреса:
// Бит 7 (OP): 0=Read, 1=Write
// Биты 6-2 (Addr): Адрес регистра (5 бит)
// Бит 1 (LABT): Last Address Bit Token = 1 (ОБЯЗАТЕЛЕН!)
// Бит 0: 1 (всегда!)

// Формула адреса: read = (addr_5bit << 2) | 0x03
//                 write = 0x80 | (addr_5bit << 2) | 0x03

#define OP_WRITE_BIT (1 << 7)
#define LABT_BIT     (1 << 1)

// Control Registers (HB_ACT_x_CTRL) - управление полумостами
#define REG_HB_ACT_1_CTRL_RD   0x03  // addr_5bit = 0b00000, HB1-4
#define REG_HB_ACT_1_CTRL_WR   0x83
#define REG_HB_ACT_2_CTRL_RD   0x43  // addr_5bit = 0b10000, HB5-8
#define REG_HB_ACT_2_CTRL_WR   0xC3
#define REG_HB_ACT_3_CTRL_RD   0x23  // addr_5bit = 0b01000, HB9-12
#define REG_HB_ACT_3_CTRL_WR   0xA3

// PWM Mode Registers (HB_MODE_x_CTRL) - выбор PWM канала
#define REG_HB_MODE_1_CTRL_RD  0x63  // addr_5bit = 0b11000, HB1-4
#define REG_HB_MODE_1_CTRL_WR  0xE3
#define REG_HB_MODE_2_CTRL_RD  0x13  // addr_5bit = 0b00100, HB5-8
#define REG_HB_MODE_2_CTRL_WR  0x93
#define REG_HB_MODE_3_CTRL_RD  0x53  // addr_5bit = 0b10100, HB9-12
#define REG_HB_MODE_3_CTRL_WR  0xD3

// PWM Control Registers - частота и duty cycle
#define REG_PWM_CH_FREQ_RD     0x33  // addr_5bit = 0b01100
#define REG_PWM_CH_FREQ_WR     0xB3
#define REG_PWM1_DC_RD         0x73  // addr_5bit = 0b11100
#define REG_PWM1_DC_WR         0xF3
#define REG_PWM2_DC_RD         0x0B  // addr_5bit = 0b00010
#define REG_PWM2_DC_WR         0x8B
#define REG_PWM3_DC_RD         0x4B  // addr_5bit = 0b10010
#define REG_PWM3_DC_WR         0xCB

// Free-wheeling and Open Load Control
#define REG_FW_OL_CTRL_RD      0x2B  // addr_5bit = 0b01010
#define REG_FW_OL_CTRL_WR      0xAB
#define REG_FW_CTRL_RD         0x6B  // addr_5bit = 0b11010
#define REG_FW_CTRL_WR         0xEB

// Configuration Register (Device ID) - READ ONLY
#define REG_CONFIG_CTRL_RD     0x67  // addr_5bit = 0b11001

// Status Registers (SYS_DIAG_x) - диагностика
#define REG_SYS_DIAG_1_RD      0x1B  // addr_5bit = 0b00110, Global status
#define REG_SYS_DIAG_1_CLR     0x9B  // write to clear
#define REG_SYS_DIAG_2_RD      0x5B  // addr_5bit = 0b10110, Overcurrent HB1-4
#define REG_SYS_DIAG_2_CLR     0xDB
#define REG_SYS_DIAG_3_RD      0x3B  // addr_5bit = 0b01110, Overcurrent HB5-8
#define REG_SYS_DIAG_3_CLR     0xBB
#define REG_SYS_DIAG_4_RD      0x7B  // addr_5bit = 0b11110, Overcurrent HB9-12
#define REG_SYS_DIAG_4_CLR     0xFB
#define REG_SYS_DIAG_5_RD      0x07  // addr_5bit = 0b00001, Open Load HB1-4
#define REG_SYS_DIAG_5_CLR     0x87
#define REG_SYS_DIAG_6_RD      0x47  // addr_5bit = 0b10001, Open Load HB5-8
#define REG_SYS_DIAG_6_CLR     0xC7
#define REG_SYS_DIAG_7_RD      0x27  // addr_5bit = 0b01001, Open Load HB9-12
#define REG_SYS_DIAG_7_CLR     0xA7

// Маски для SYS_DIAG_1
#define SYS_DIAG_SPI_ERR   (1 << 7)
#define SYS_DIAG_LE        (1 << 6)
#define SYS_DIAG_VS_UV     (1 << 5)
#define SYS_DIAG_VS_OV     (1 << 4)
#define SYS_DIAG_NPOR      (1 << 3)
#define SYS_DIAG_TSD       (1 << 2)
#define SYS_DIAG_TPW       (1 << 1)

// Маски для CONFIG_CTRL (Device ID)
#define DEV_ID_MASK        0x07
#define DEV_ID_SHIFT       0

struct tle94112_handle_s {
    spi_device_handle_t spi_dev;
    SemaphoreHandle_t mutex;
    int en_gpio;
    uint8_t ctrl_cache[3]; // Кеш состояний для CTRL_1, CTRL_2, CTRL_3
    tle94112_protection_config_t protection_config; // Конфигурация защит
    tle_fault_reason_t last_fault_reason; // Последняя ошибка
};

// === Вспомогательные функции SPI ===

// Выполнение 16-битной транзакции
static esp_err_t spi_transaction(tle94112_handle_t handle, uint8_t addr_byte, uint8_t data_byte, uint8_t *rx_data) {
    uint8_t tx_buf[2] = { addr_byte, data_byte };
    uint8_t rx_buf[2] = { 0 };

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
        .flags = 0, // LSB-first уже задан в device config
    };

    esp_err_t ret = spi_device_transmit(handle->spi_dev, &t);
    if (ret != ESP_OK) return ret;

    // rx_buf[0] - Global Status (возвращается всегда, но мы его пока игнорируем)
    // rx_buf[1] - Данные регистра (при чтении)
    if (rx_data) *rx_data = rx_buf[1];
    
    return ESP_OK;
}

// === Публичный API ===

tle94112_handle_t tle94112_init(const tle94112_config_t *config) {
    if (!config) return NULL;

    // 1. Выделение памяти
    tle94112_handle_t handle = calloc(1, sizeof(struct tle94112_handle_s));
    if (!handle) return NULL;

    handle->en_gpio = config->gpio_en;
    handle->mutex = xSemaphoreCreateMutex();

    // 2. Настройка GPIO EN
    if (handle->en_gpio >= 0) {
        gpio_reset_pin(handle->en_gpio);
        gpio_set_direction(handle->en_gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(handle->en_gpio, 0); // OFF initially
    }

    // 3. Настройка SPI (Mode 1, LSB-first!)
    // TLE94112 требует LSB-first (см. datasheet Figure 19)
    spi_device_interface_config_t devcfg = {
        .mode = 1,                  // CPOL=0, CPHA=1
        .clock_speed_hz = config->spi_freq_hz,
        .spics_io_num = config->gpio_cs,
        .queue_size = 3,
        .flags = SPI_DEVICE_BIT_LSBFIRST,  // LSB-first!
    };
    
    if (spi_bus_add_device(config->spi_host, &devcfg, &handle->spi_dev) != ESP_OK) {
        free(handle);
        return NULL;
    }

    // 4. Включение чипа
    if (handle->en_gpio >= 0) {
        gpio_set_level(handle->en_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(10)); // Время выхода из Sleep
    }

    // 5. Проверка Device ID (чтение CONFIG_CTRL)
    // Device ID находится в регистре CONFIG_CTRL, биты D2-D0
    uint8_t config_data = 0;
    esp_err_t ret = spi_transaction(handle, REG_CONFIG_CTRL_RD, 0x00, &config_data);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
        spi_bus_remove_device(handle->spi_dev);
        free(handle);
        return NULL;
    }

    ESP_LOGI(TAG, "CONFIG_CTRL: 0x%02X", config_data);
    uint8_t dev_id = config_data & DEV_ID_MASK;

    // === ПРОВЕРКА ID ===
    // Согласно datasheet (стр. 64):
    // 0x0 = TLE94112EL (12 half-bridges)
    // 0x1 = TLE94110EL (10 half-bridges)
    // 0x2 = TLE94108EL (8 half-bridges)
    // 0x3 = TLE94106EL (6 half-bridges)
    // 0x4 = TLE94104EP (4 half-bridges)
    // 0x5 = TLE94103EP (3 half-bridges)
    // Примечание: некоторые ревизии чипов могут иметь другие ID
    const char* chip_names[] = {
        "TLE94112EL", "TLE94110EL", "TLE94108EL", 
        "TLE94106EL", "TLE94104EP", "TLE94103EP"
    };
    
    // Разрешаем любой ID (для тестирования)
    if (dev_id > 0x05) {
        ESP_LOGW(TAG, "Unknown Device ID: 0x%X (assuming TLE94112EL)", dev_id);
        dev_id = 0;  // Предполагаем TLE94112EL
    }

    ESP_LOGI(TAG, "Detected: %s (ID=0x%X)", chip_names[dev_id], dev_id);

    // 6. Сброс регистров и кеша
    handle->ctrl_cache[0] = 0;
    handle->ctrl_cache[1] = 0;
    handle->ctrl_cache[2] = 0;
    
    // Инициализация конфигурации защиты по умолчанию
    handle->protection_config = (tle94112_protection_config_t)TLE94112_PROTECTION_DEFAULTS;
    handle->last_fault_reason = TLE_FAULT_NONE;

    spi_transaction(handle, REG_HB_ACT_1_CTRL_WR, 0, NULL);
    spi_transaction(handle, REG_HB_ACT_2_CTRL_WR, 0, NULL);
    spi_transaction(handle, REG_HB_ACT_3_CTRL_WR, 0, NULL);

    // Отключение детектора Open Load (чтобы в паузах вызова не возникал LE) и пассивный freewheel
    spi_transaction(handle, REG_FW_OL_CTRL_WR, 0x00, NULL);
    spi_transaction(handle, REG_FW_CTRL_WR, 0x00, NULL);

    // 7. Очистка всех статусных регистров ошибок
    tle94112_clear_errors(handle);
    ESP_LOGI(TAG, "Initialized, Open Load disabled, and all errors cleared.");

    return handle;
}

void tle94112_deinit(tle94112_handle_t handle) {
    if (!handle) return;
    
    if (handle->en_gpio >= 0) gpio_set_level(handle->en_gpio, 0);
    
    spi_bus_remove_device(handle->spi_dev);
    vSemaphoreDelete(handle->mutex);
    free(handle);
}

esp_err_t tle94112_set_hb_state(tle94112_handle_t handle, tle_hb_idx_t hb, tle_hb_state_t state) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    if ((unsigned)hb >= 12U || (unsigned)state > (unsigned)TLE_HB_BRAKE) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    uint8_t reg_idx;
    uint8_t bit_pos;
    uint8_t addr;

    // Маппинг HB на регистры
    if (hb <= TLE_HB4) {
        reg_idx = 0; addr = REG_HB_ACT_1_CTRL_WR;
    } else if (hb <= TLE_HB8) {
        reg_idx = 1; addr = REG_HB_ACT_2_CTRL_WR; hb = (tle_hb_idx_t)(hb - 4); 
    } else {
        reg_idx = 2; addr = REG_HB_ACT_3_CTRL_WR; hb = (tle_hb_idx_t)(hb - 8);
    }

    bit_pos = hb * 2;

    // Read-Modify-Write (используем кеш)
    uint8_t current_val = handle->ctrl_cache[reg_idx];
    current_val &= ~(0x03 << bit_pos);      
    current_val |= ((uint8_t)state << bit_pos); 

    // Защита: полумост HB2 подключен к линии OpenD (открытие замка двери).
    // Гарантируем, что HB2 принудительно выключен в Low-Side при коммутации квартир!
    if (reg_idx == 0 && hb != TLE_HB2) {
        current_val &= ~(0x03 << (TLE_HB2 * 2));
    }

    // Сначала отправляем команду. Кеш обновляем только после успешной передачи.
    esp_err_t ret = spi_transaction(handle, addr, current_val, NULL);
    if (ret == ESP_OK) {
        handle->ctrl_cache[reg_idx] = current_val;
    } else {
        ESP_LOGE(TAG, "HB control write failed (register 0x%02X): %s",
                 addr, esp_err_to_name(ret));
    }

    // Диагностические флаги здесь НЕ очищаем: они должны оставаться доступны
    // приложению до явного вызова tle94112_clear_errors() после фиксации причины.
    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t tle94112_set_all_hb_state(tle94112_handle_t handle, tle_hb_state_t state) {
    if (!handle || (unsigned)state > (unsigned)TLE_HB_BRAKE) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    uint8_t val = 0;
    // Формируем байт, где каждая пара бит = state
    for (int i = 0; i < 4; i++) {
        val |= ((uint8_t)state << (i * 2));
    }

    // Обновляем кеш только для тех банков, запись которых подтверждена SPI.
    esp_err_t ret = spi_transaction(handle, REG_HB_ACT_1_CTRL_WR, val, NULL);
    if (ret == ESP_OK) {
        handle->ctrl_cache[0] = val;
        ret = spi_transaction(handle, REG_HB_ACT_2_CTRL_WR, val, NULL);
    }
    if (ret == ESP_OK) {
        handle->ctrl_cache[1] = val;
        ret = spi_transaction(handle, REG_HB_ACT_3_CTRL_WR, val, NULL);
    }
    if (ret == ESP_OK) {
        handle->ctrl_cache[2] = val;
    } else {
        ESP_LOGE(TAG, "Failed to set all half-bridges: %s", esp_err_to_name(ret));
    }

    // Не стираем диагностические регистры как побочный эффект управления выходами.
    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t tle94112_clear_errors(tle94112_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;

    static const uint8_t clear_regs[] = {
        REG_SYS_DIAG_1_CLR, REG_SYS_DIAG_2_CLR, REG_SYS_DIAG_3_CLR,
        REG_SYS_DIAG_4_CLR, REG_SYS_DIAG_5_CLR, REG_SYS_DIAG_6_CLR,
        REG_SYS_DIAG_7_CLR
    };

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < sizeof(clear_regs) / sizeof(clear_regs[0]); ++i) {
        ret = spi_transaction(handle, clear_regs[i], 0x00, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear diagnostic register 0x%02X: %s",
                     clear_regs[i], esp_err_to_name(ret));
            break;
        }
    }
    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t tle94112_diagnose(tle94112_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t sys_diag = 0;
    esp_err_t ret = spi_transaction(handle, REG_SYS_DIAG_1_RD, 0x00, &sys_diag);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "SYS_DIAG_1: 0x%02X", sys_diag);

    // Биты SYS_DIAG_1 согласно datasheet стр. 66:
    // D7: SPI_ERR - SPI protocol error
    // D6: LE - Load Error (OC or OL)
    // D5: VS_UV - Undervoltage
    // D4: VS_OV - Overvoltage
    // D3: NPOR - Not Power On Reset
    // D2: TSD - Thermal Shutdown
    // D1: TPW - Temperature Pre-Warning
    if (sys_diag & SYS_DIAG_SPI_ERR) ESP_LOGE(TAG, "SPI_ERROR");
    if (sys_diag & SYS_DIAG_LE)      ESP_LOGE(TAG, "LE (Load Error: OC or OL)");
    if (sys_diag & SYS_DIAG_VS_UV)   ESP_LOGE(TAG, "VS_UV (Undervoltage)");
    if (sys_diag & SYS_DIAG_VS_OV)   ESP_LOGE(TAG, "VS_OV (Overvoltage)");
    if (sys_diag & SYS_DIAG_NPOR)    ESP_LOGW(TAG, "NPOR (No Power-On Reset)");
    if (sys_diag & SYS_DIAG_TSD)     ESP_LOGE(TAG, "TSD (Thermal Shutdown)");
    if (sys_diag & SYS_DIAG_TPW)     ESP_LOGW(TAG, "TPW (Temperature Pre-Warning)");
    
    return ESP_OK;
}

esp_err_t tle94112_get_sys_diag1(tle94112_handle_t handle, uint8_t *sys_diag_out) {
    if (!handle || !sys_diag_out) return ESP_ERR_INVALID_ARG;
    return spi_transaction(handle, REG_SYS_DIAG_1_RD, 0x00, sys_diag_out);
}

esp_err_t tle94112_read_all_diag(tle94112_handle_t handle, uint8_t diag[7]) {
    if (!handle || !diag) return ESP_ERR_INVALID_ARG;
    static const uint8_t regs[] = {
        REG_SYS_DIAG_1_RD, REG_SYS_DIAG_2_RD, REG_SYS_DIAG_3_RD,
        REG_SYS_DIAG_4_RD, REG_SYS_DIAG_5_RD, REG_SYS_DIAG_6_RD,
        REG_SYS_DIAG_7_RD
    };
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
        ret = spi_transaction(handle, regs[i], 0, &diag[i]);
        if (ret != ESP_OK) break;
    }
    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t tle94112_read_all_act(tle94112_handle_t handle, uint8_t act[3]) {
    if (!handle || !act) return ESP_ERR_INVALID_ARG;
    static const uint8_t regs[] = {
        REG_HB_ACT_1_CTRL_RD, REG_HB_ACT_2_CTRL_RD, REG_HB_ACT_3_CTRL_RD
    };
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i) {
        ret = spi_transaction(handle, regs[i], 0, &act[i]);
        if (ret != ESP_OK) break;
    }
    xSemaphoreGive(handle->mutex);
    return ret;
}

esp_err_t tle94112_get_overcurrent(tle94112_handle_t handle, uint16_t *oc_mask) {
    if (!handle || !oc_mask) return ESP_ERR_INVALID_ARG;

    *oc_mask = 0;
    uint8_t diag2 = 0, diag3 = 0, diag4 = 0;

    // Чтение SYS_DIAG_2 (OC HB1-4), SYS_DIAG_3 (OC HB5-8), SYS_DIAG_4 (OC HB9-12)
    // Каждый регистр: 2 бита на HB, бит 0 = OC_HS, бит 1 = OC_LS
    // Если любой бит = 1, значит есть overcurrent
    
    esp_err_t ret;
    ret = spi_transaction(handle, REG_SYS_DIAG_2_RD, 0x00, &diag2);
    if (ret != ESP_OK) return ret;
    
    ret = spi_transaction(handle, REG_SYS_DIAG_3_RD, 0x00, &diag3);
    if (ret != ESP_OK) return ret;
    
    ret = spi_transaction(handle, REG_SYS_DIAG_4_RD, 0x00, &diag4);
    if (ret != ESP_OK) return ret;

    // Защита от 0xFF: если чип в слипе / шина свободна / MISO подтянут к VDD,
    // все считанные байты равны 0xFF. Это означает отсутствие ответа, а не перегрузку всех 12 полумостов!
    if (diag2 == 0xFF && diag3 == 0xFF && diag4 == 0xFF) {
        *oc_mask = 0;
        return ESP_OK;
    }

    // Формируем маску: бит N = 1 если HB(N+1) имеет OC
    // SYS_DIAG_2: биты [1:0]=HB1, [3:2]=HB2, [5:4]=HB3, [7:6]=HB4
    for (int i = 0; i < 4; i++) {
        uint8_t hb_bits = (diag2 >> (i * 2)) & 0x03;
        if (hb_bits) *oc_mask |= (1 << i);
    }

    // SYS_DIAG_3: HB5-8
    for (int i = 0; i < 4; i++) {
        uint8_t hb_bits = (diag3 >> (i * 2)) & 0x03;
        if (hb_bits) *oc_mask |= (1 << (i + 4));
    }

    // SYS_DIAG_4: HB9-12
    for (int i = 0; i < 4; i++) {
        uint8_t hb_bits = (diag4 >> (i * 2)) & 0x03;
        if (hb_bits) *oc_mask |= (1 << (i + 8));
    }

    return ESP_OK;
}

esp_err_t tle94112_get_openload(tle94112_handle_t handle, uint16_t *ol_mask) {
    if (!handle || !ol_mask) return ESP_ERR_INVALID_ARG;

    *ol_mask = 0;
    uint8_t diag5 = 0, diag6 = 0, diag7 = 0;

    // Чтение SYS_DIAG_5 (OL HB1-4), SYS_DIAG_6 (OL HB5-8), SYS_DIAG_7 (OL HB9-12)
    esp_err_t ret;
    ret = spi_transaction(handle, REG_SYS_DIAG_5_RD, 0x00, &diag5);
    if (ret != ESP_OK) return ret;
    
    ret = spi_transaction(handle, REG_SYS_DIAG_6_RD, 0x00, &diag6);
    if (ret != ESP_OK) return ret;
    
    ret = spi_transaction(handle, REG_SYS_DIAG_7_RD, 0x00, &diag7);
    if (ret != ESP_OK) return ret;

    // Защита от 0xFF: если чип в слипе / шина свободна, не считаем это обрывом на всех каналах
    if (diag5 == 0xFF && diag6 == 0xFF && diag7 == 0xFF) {
        *ol_mask = 0;
        return ESP_OK;
    }

    // Формируем маску: бит N = 1 если HB(N+1) имеет OL
    for (int i = 0; i < 4; i++) {
        uint8_t hb_bits = (diag5 >> (i * 2)) & 0x03;
        if (hb_bits) *ol_mask |= (1 << i);
    }

    for (int i = 0; i < 4; i++) {
        uint8_t hb_bits = (diag6 >> (i * 2)) & 0x03;
        if (hb_bits) *ol_mask |= (1 << (i + 4));
    }

    for (int i = 0; i < 4; i++) {
        uint8_t hb_bits = (diag7 >> (i * 2)) & 0x03;
        if (hb_bits) *ol_mask |= (1 << (i + 8));
    }

    return ESP_OK;
}

esp_err_t tle94112_clear_overcurrent(tle94112_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = spi_transaction(handle, REG_SYS_DIAG_2_CLR, 0x00, NULL);
    if (ret != ESP_OK) return ret;
    ret = spi_transaction(handle, REG_SYS_DIAG_3_CLR, 0x00, NULL);
    if (ret != ESP_OK) return ret;
    return spi_transaction(handle, REG_SYS_DIAG_4_CLR, 0x00, NULL);
}

esp_err_t tle94112_clear_openload(tle94112_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = spi_transaction(handle, REG_SYS_DIAG_5_CLR, 0x00, NULL);
    if (ret != ESP_OK) return ret;
    ret = spi_transaction(handle, REG_SYS_DIAG_6_CLR, 0x00, NULL);
    if (ret != ESP_OK) return ret;
    return spi_transaction(handle, REG_SYS_DIAG_7_CLR, 0x00, NULL);
}

esp_err_t tle94112_full_diagnose(tle94112_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;

    // Глобальная диагностика
    tle94112_diagnose(handle);

    // Overcurrent
    uint16_t oc_mask = 0;
    tle94112_get_overcurrent(handle, &oc_mask);
    if (oc_mask) {
        ESP_LOGE(TAG, "Overcurrent detected:");
        for (int i = 0; i < 12; i++) {
            if (oc_mask & (1 << i)) {
                ESP_LOGE(TAG, "  HB%d: OVERCURRENT", i + 1);
            }
        }
    } else {
        ESP_LOGI(TAG, "No overcurrent");
    }

    // Open Load
    uint16_t ol_mask = 0;
    tle94112_get_openload(handle, &ol_mask);
    if (ol_mask) {
        ESP_LOGW(TAG, "Open Load detected:");
        for (int i = 0; i < 12; i++) {
            if (ol_mask & (1 << i)) {
                ESP_LOGW(TAG, "  HB%d: OPEN LOAD", i + 1);
            }
        }
    } else {
        ESP_LOGI(TAG, "No open load");
    }

    return ESP_OK;
}

// === Функции системы защиты ===

esp_err_t tle94112_set_protection_config(tle94112_handle_t handle, 
                                         const tle94112_protection_config_t *config) {
    if (!handle || !config) return ESP_ERR_INVALID_ARG;
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    memcpy(&handle->protection_config, config, sizeof(tle94112_protection_config_t));
    handle->last_fault_reason = TLE_FAULT_NONE; // Сброс истории ошибок
    xSemaphoreGive(handle->mutex);
    
    ESP_LOGI(TAG, "Protection config updated: OC=%d, UV=%d, OV=%d, Thermal=%d, OL=%d",
             config->enable_overcurrent, config->enable_undervoltage, 
             config->enable_overvoltage, config->enable_thermal, config->enable_openload);
    
    return ESP_OK;
}

tle_fault_reason_t tle94112_check_protection(tle94112_handle_t handle, uint16_t voltage_mv) {
    if (!handle) return TLE_FAULT_SPI_ERROR;
    
    tle_fault_reason_t fault = TLE_FAULT_NONE;
    const tle94112_protection_config_t *cfg = &handle->protection_config;
    
    // 1. Проверка overcurrent (аппаратная)
    if (cfg->enable_overcurrent) {
        uint16_t oc_mask = 0;
        if (tle94112_get_overcurrent(handle, &oc_mask) == ESP_OK) {
            // Проверяем только биты из маски
            oc_mask &= cfg->overcurrent_mask;
            if (oc_mask) {
                ESP_LOGE(TAG, "OVERCURRENT detected! Mask: 0x%04X", oc_mask);
                fault = TLE_FAULT_OVERCURRENT;
            }
        }
    }
    
    // 2. Проверка open load (аппаратная)
    if (cfg->enable_openload && fault == TLE_FAULT_NONE) {
        uint16_t ol_mask = 0;
        if (tle94112_get_openload(handle, &ol_mask) == ESP_OK) {
            if (ol_mask) {
                ESP_LOGW(TAG, "OPEN LOAD detected! Mask: 0x%04X", ol_mask);
                fault = TLE_FAULT_OPEN_LOAD;
            }
        }
    }
    
    // 3. Проверка напряжения (от INA226)
    if (voltage_mv > 0) {
        if (cfg->enable_overvoltage && cfg->overvoltage_mv > 0) {
            if (voltage_mv >= cfg->overvoltage_mv) {
                ESP_LOGE(TAG, "OVERVOLTAGE: %d mV >= %d mV", voltage_mv, cfg->overvoltage_mv);
                fault = TLE_FAULT_OVERVOLTAGE;
            }
        }
        
        if (cfg->enable_undervoltage && cfg->undervoltage_mv > 0) {
            if (voltage_mv <= cfg->undervoltage_mv) {
                ESP_LOGE(TAG, "UNDERVOLTAGE: %d mV <= %d mV", voltage_mv, cfg->undervoltage_mv);
                fault = TLE_FAULT_UNDERVOLTAGE;
            }
        }
    }
    
    // 4. Проверка тепловой защиты (аппаратная)
    if (cfg->enable_thermal && fault == TLE_FAULT_NONE) {
        uint8_t sys_diag = 0;
        spi_transaction(handle, REG_SYS_DIAG_1_RD, 0x00, &sys_diag);
        
        if (sys_diag & SYS_DIAG_TSD) {
            ESP_LOGE(TAG, "THERMAL SHUTDOWN!");
            fault = TLE_FAULT_THERMAL_SHUTDOWN;
        } else if (sys_diag & SYS_DIAG_TPW) {
            ESP_LOGW(TAG, "THERMAL PRE-WARNING");
            fault = TLE_FAULT_THERMAL_WARNING;
        }
    }
    
    // 5. Проверка SPI ошибок
    if (fault == TLE_FAULT_NONE) {
        uint8_t sys_diag = 0;
        spi_transaction(handle, REG_SYS_DIAG_1_RD, 0x00, &sys_diag);
        if (sys_diag & SYS_DIAG_SPI_ERR) {
            ESP_LOGE(TAG, "SPI ERROR!");
            fault = TLE_FAULT_SPI_ERROR;
        }
    }
    
    // Сохраняем последнюю ошибку
    handle->last_fault_reason = fault;
    
    // 6. Аварийное отключение при критической ошибке
    if (fault != TLE_FAULT_NONE && fault != TLE_FAULT_THERMAL_WARNING && 
        cfg->auto_stop_on_fault) {
        ESP_LOGW(TAG, "AUTO-STOP on fault %d", fault);
        tle94112_emergency_stop(handle);
    }
    
    return fault;
}

esp_err_t tle94112_emergency_stop(tle94112_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    
    ESP_LOGW(TAG, "EMERGENCY STOP!");
    
    // Отключаем все выходы
    handle->ctrl_cache[0] = 0;
    handle->ctrl_cache[1] = 0;
    handle->ctrl_cache[2] = 0;
    
    // Быстрая отправка команд отключения
    spi_transaction(handle, REG_HB_ACT_1_CTRL_WR, 0, NULL);
    spi_transaction(handle, REG_HB_ACT_2_CTRL_WR, 0, NULL);
    spi_transaction(handle, REG_HB_ACT_3_CTRL_WR, 0, NULL);
    
    // Очистка всех флагов ошибок
    spi_transaction(handle, REG_SYS_DIAG_1_CLR, 0x00, NULL);
    spi_transaction(handle, REG_SYS_DIAG_2_CLR, 0x00, NULL);
    spi_transaction(handle, REG_SYS_DIAG_3_CLR, 0x00, NULL);
    spi_transaction(handle, REG_SYS_DIAG_4_CLR, 0x00, NULL);
    spi_transaction(handle, REG_SYS_DIAG_5_CLR, 0x00, NULL);
    spi_transaction(handle, REG_SYS_DIAG_6_CLR, 0x00, NULL);
    spi_transaction(handle, REG_SYS_DIAG_7_CLR, 0x00, NULL);
    
    xSemaphoreGive(handle->mutex);
    
    return ESP_OK;
}

tle_fault_reason_t tle94112_get_last_fault_reason(tle94112_handle_t handle) {
    if (!handle) return TLE_FAULT_NONE;
    return handle->last_fault_reason;
}