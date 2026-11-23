/**
 * @file levisw_mb_v1_2_bsp.c
 * @brief Реализация пакета поддержки платы (BSP) LeviSW-MB v1.2 для ESP-IDF (>= 5.0).
 */

#include "levisw_mb_v1_2_bsp.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "levisw_bsp";

/* Таймаут транзакций I2C (мс) */
#define LEVISW_I2C_TIMEOUT_MS       50

/* Регистры расширителя портов TI TCA9535PWR (I2C 0x20) */
#define TCA9535_REG_INPUT_P0        0x00
#define TCA9535_REG_INPUT_P1        0x01
#define TCA9535_REG_OUTPUT_P0       0x02
#define TCA9535_REG_OUTPUT_P1       0x03
#define TCA9535_REG_POLARITY_P0     0x04
#define TCA9535_REG_POLARITY_P1     0x05
#define TCA9535_REG_CONFIG_P0       0x06
#define TCA9535_REG_CONFIG_P1       0x07

/* Команды Microchip MCP4532 (энергозависимый регистр Wiper 0, адрес 0x0) */
#define MCP4532_CMD_WRITE_W0(d8)    ((uint8_t)(0x00U | ((d8) & 0x01U)))
#define MCP4532_CMD_READ_W0         0x0CU
#define MCP4532_MAX_WIPER           128U

/* Регистры измерителя мощности TI INA226AIDGSR */
#define INA226_REG_CONFIG           0x00
#define INA226_REG_SHUNT_V          0x01
#define INA226_REG_BUS_V            0x02
#define INA226_REG_POWER            0x03
#define INA226_REG_CURRENT          0x04
#define INA226_REG_CALIBRATION      0x05

/*
 * Конфигурация INA226:
 *  - Усреднение: 4 выборки (AVG = 001b -> 0x0200)
 *  - Время преобразования VBUS = 1.1 мс (100b), VSHUNT = 1.1 мс (100b)
 *  - Режим: непрерывное измерение шунта и шины (111b)
 *  -> 0x4327
 * Калибровка под шунт R_shunt = 20 мОм (0.020 Ом) и Current_LSB = 0.1 мА (0.0001 А):
 *  CAL = 0.00512 / (Current_LSB * R_shunt) = 0.00512 / (0.0001 * 0.020) = 2560 (0x0A00)
 *  Power_LSB = 25 * Current_LSB = 2.5 мВт
 *  Bus_LSB   = 1.25 мВ
 *  Shunt_LSB = 2.5 мкВ (0.0025 мВ)
 */
#define INA226_CONFIG_DEFAULT       0x4327U
#define INA226_CAL_20MOHM_0_1MA     2560U
#define INA226_BUS_LSB_V            0.00125f
#define INA226_SHUNT_LSB_MV         0.0025f
#define INA226_CURRENT_LSB_MA       0.1f
#define INA226_POWER_LSB_MW         2.5f

/* Значения Kconfig с запасными значениями по умолчанию */
#ifndef CONFIG_LEVISW_I2C_FREQ_HZ
#define CONFIG_LEVISW_I2C_FREQ_HZ           400000
#endif
#ifndef CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ
#define CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ  2000000
#endif
#ifndef CONFIG_LEVISW_SPI_MC33879_FREQ_HZ
#define CONFIG_LEVISW_SPI_MC33879_FREQ_HZ   2000000
#endif
#ifndef CONFIG_LEVISW_DEFAULT_V1_WIPER
#define CONFIG_LEVISW_DEFAULT_V1_WIPER      2
#endif
#ifndef CONFIG_LEVISW_DEFAULT_V2_WIPER
#define CONFIG_LEVISW_DEFAULT_V2_WIPER      10
#endif
#ifndef CONFIG_LEVISW_DEFAULT_ULOAD_WIPER
#define CONFIG_LEVISW_DEFAULT_ULOAD_WIPER   64
#endif

#define LEVISW_ADC_ATTEN ADC_ATTEN_DB_12

typedef struct {
    bool                 initialized;
    bool                 spi_bus_init;
    levisw_bsp_handles_t handles;
    SemaphoreHandle_t    exp_mutex;
    uint8_t              exp_out_p0;
    uint8_t              exp_out_p1;
    adc_cali_handle_t    adc1_cali;
} levisw_bsp_state_t;

static levisw_bsp_state_t s_bsp = {0};

/* --------------------------------------------------------------------------
 * Вспомогательные функции работы с регистрами I2C
 * ------------------------------------------------------------------------ */

static esp_err_t i2c_write_reg8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev, buf, sizeof(buf), LEVISW_I2C_TIMEOUT_MS);
}

static esp_err_t i2c_read_reg8(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(dev, &reg, 1, val, 1, LEVISW_I2C_TIMEOUT_MS);
}

static esp_err_t i2c_write_reg16_be(i2c_master_dev_handle_t dev, uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = {
        reg,
        (uint8_t)((val >> 8) & 0xFFU),
        (uint8_t)(val & 0xFFU)
    };
    return i2c_master_transmit(dev, buf, sizeof(buf), LEVISW_I2C_TIMEOUT_MS);
}

static esp_err_t i2c_read_reg16_be(i2c_master_dev_handle_t dev, uint8_t reg, uint16_t *val)
{
    uint8_t rx[2] = {0};
    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, rx, sizeof(rx), LEVISW_I2C_TIMEOUT_MS);
    if (err == ESP_OK) {
        *val = ((uint16_t)rx[0] << 8) | (uint16_t)rx[1];
    }
    return err;
}

static i2c_master_dev_handle_t pot_handle_from_bus(levisw_power_bus_t bus)
{
    switch (bus) {
    case LEVISW_BUS_V1:
        return s_bsp.handles.i2c_pot_v1;
    case LEVISW_BUS_V2:
        return s_bsp.handles.i2c_pot_v2;
    case LEVISW_BUS_ULOAD:
        return s_bsp.handles.i2c_pot_uload;
    default:
        return NULL;
    }
}

static i2c_master_dev_handle_t ina_handle_from_ch(levisw_ina_channel_t ch)
{
    switch (ch) {
    case LEVISW_INA_V1:
        return s_bsp.handles.i2c_ina_v1;
    case LEVISW_INA_V2:
        return s_bsp.handles.i2c_ina_v2;
    default:
        return NULL;
    }
}

static esp_err_t add_i2c_device(uint16_t addr, i2c_master_dev_handle_t *out_dev)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = CONFIG_LEVISW_I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(s_bsp.handles.i2c_bus, &dev_cfg, out_dev);
}

/* --------------------------------------------------------------------------
 * Инициализация и деинициализация подсистем
 * ------------------------------------------------------------------------ */

static esp_err_t init_gpio(void)
{
    /* Сначала конфигурируем GPIO как выходы, затем задаём безопасные уровни.
     * EN-DE=0 отключает силовые драйверы; RS-ON=0 удерживает MAX13487 в shutdown.
     * GPIO27 имеет общие цепи BUTTON/D8-A5 — активировать RS-485 следует явно из приложения. */
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << LEVISW_PIN_SPI_EN2_DRIVERS) |
                        (1ULL << LEVISW_PIN_RS_ON),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&out_cfg), TAG, "Ошибка настройки выходных GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(LEVISW_PIN_SPI_EN2_DRIVERS, 0),
                        TAG, "Не удалось удержать EN-DE в LOW");
    ESP_RETURN_ON_ERROR(gpio_set_level(LEVISW_PIN_RS_ON, 0),
                        TAG, "Не удалось удержать RS-ON в LOW");

    /* Входы детекции линий трубок LU1/LU2 и линии прерываний I2C */
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << LEVISW_PIN_LU1_DO1) |
                        (1ULL << LEVISW_PIN_LU2_DO2) |
                        (1ULL << LEVISW_PIN_INT0_TCA9535) |
                        (1ULL << LEVISW_PIN_INT1_INA_MV1) |
                        (1ULL << LEVISW_PIN_INT2_INA_MV2),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&in_cfg), TAG, "Ошибка настройки входных GPIO");

    return ESP_OK;
}

static esp_err_t init_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = LEVISW_PIN_I2C_SDA,
        .scl_io_num        = LEVISW_PIN_I2C_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false, /* На плате установлены внешние подтяжки RN15 10k */
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bsp.handles.i2c_bus),
                        TAG, "Ошибка создания шины I2C-L");

    ESP_RETURN_ON_ERROR(add_i2c_device(LEVISW_I2C_ADDR_TCA9535, &s_bsp.handles.i2c_tca9535),
                        TAG, "Не удалось добавить DD3 TCA9535 (0x20)");
    ESP_RETURN_ON_ERROR(add_i2c_device(LEVISW_I2C_ADDR_POT_V1, &s_bsp.handles.i2c_pot_v1),
                        TAG, "Не удалось добавить DA2 MCP4532 (0x2C)");
    ESP_RETURN_ON_ERROR(add_i2c_device(LEVISW_I2C_ADDR_POT_V2, &s_bsp.handles.i2c_pot_v2),
                        TAG, "Не удалось добавить DA3 MCP4532 (0x2D)");
    ESP_RETURN_ON_ERROR(add_i2c_device(LEVISW_I2C_ADDR_POT_ULOAD, &s_bsp.handles.i2c_pot_uload),
                        TAG, "Не удалось добавить DA4 MCP4532 (0x2F)");
    ESP_RETURN_ON_ERROR(add_i2c_device(LEVISW_I2C_ADDR_INA_V1, &s_bsp.handles.i2c_ina_v1),
                        TAG, "Не удалось добавить DD5 INA226 (0x40)");
    ESP_RETURN_ON_ERROR(add_i2c_device(LEVISW_I2C_ADDR_INA_V2, &s_bsp.handles.i2c_ina_v2),
                        TAG, "Не удалось добавить DD6 INA226 (0x41)");

    return ESP_OK;
}

static esp_err_t init_tca9535(void)
{
    s_bsp.exp_out_p0 = 0x00;
    s_bsp.exp_out_p1 = 0x00;

    /* Сначала устанавливаем безопасные нулевые уровни на выходах до перевода пинов в Output */
    ESP_RETURN_ON_ERROR(i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P0, s_bsp.exp_out_p0),
                        TAG, "Ошибка записи OUTPUT_P0 TCA9535");
    ESP_RETURN_ON_ERROR(i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P1, s_bsp.exp_out_p1),
                        TAG, "Ошибка записи OUTPUT_P1 TCA9535");
    ESP_RETURN_ON_ERROR(i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_POLARITY_P0, 0x00),
                        TAG, "Ошибка записи POLARITY_P0 TCA9535");
    ESP_RETURN_ON_ERROR(i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_POLARITY_P1, 0x00),
                        TAG, "Ошибка записи POLARITY_P1 TCA9535");

    /* Настройка направлений: 1 = вход (PG на P0.1, ALARMIN на P1.5), 0 = выход */
    ESP_RETURN_ON_ERROR(i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_CONFIG_P0, LEVISW_EXP_P0_DIR_MASK),
                        TAG, "Ошибка записи CONFIG_P0 TCA9535");
    ESP_RETURN_ON_ERROR(i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_CONFIG_P1, LEVISW_EXP_P1_DIR_MASK),
                        TAG, "Ошибка записи CONFIG_P1 TCA9535");

    return ESP_OK;
}

static esp_err_t init_spi(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LEVISW_PIN_SPI_MOSI,
        .miso_io_num     = LEVISW_PIN_SPI_MISO,
        .sclk_io_num     = LEVISW_PIN_SPI_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 16,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED),
                        TAG, "Ошибка инициализации шины SPI2");
    s_bsp.spi_bus_init = true;

    /* D2 TLE94112EL: SPI Mode 1 (CPOL=0, CPHA=1), CS1 = IO10 */
    spi_device_interface_config_t tle_cfg = {
        .mode           = 1,
        .clock_speed_hz = CONFIG_LEVISW_SPI_TLE94112_FREQ_HZ,
        .spics_io_num   = LEVISW_PIN_SPI_CS1_TLE,
        .queue_size     = 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI2_HOST, &tle_cfg, &s_bsp.handles.spi_tle94112),
                        TAG, "Ошибка добавления SPI-устройства TLE94112EL");

    /* D1 MC33879APEKR2: SPI Mode 1 (CPOL=0, CPHA=1), CS2 = IO8 */
    spi_device_interface_config_t mc33_cfg = {
        .mode             = 1,
        .clock_speed_hz   = CONFIG_LEVISW_SPI_MC33879_FREQ_HZ,
        .spics_io_num     = LEVISW_PIN_SPI_CS2_MC33,
        .queue_size       = 2,
        .cs_ena_pretrans  = 2,
        .cs_ena_posttrans = 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI2_HOST, &mc33_cfg, &s_bsp.handles.spi_mc33879),
                        TAG, "Ошибка добавления SPI-устройства MC33879");

    return ESP_OK;
}

static esp_err_t init_adc(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_bsp.handles.adc1_handle),
                        TAG, "Ошибка инициализации ADC1");

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = LEVISW_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_bsp.handles.adc1_handle, LEVISW_ADC_CH_VAMP, &ch_cfg),
                        TAG, "Ошибка настройки канала ADC1_CH6 (VAmp)");

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = LEVISW_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &s_bsp.adc1_cali) != ESP_OK) {
        s_bsp.adc1_cali = NULL;
        ESP_LOGW(TAG, "Калибровка ADC1 недоступна, используется линейный расчёт");
    }
#endif

    return ESP_OK;
}

esp_err_t levisw_bsp_init(void)
{
    if (s_bsp.initialized) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;

    s_bsp.exp_mutex = xSemaphoreCreateMutex();
    if (s_bsp.exp_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_GOTO_ON_ERROR(init_gpio(), cleanup, TAG, "init_gpio failed");
    ESP_GOTO_ON_ERROR(init_i2c(), cleanup, TAG, "init_i2c failed");
    ESP_GOTO_ON_ERROR(init_tca9535(), cleanup, TAG, "init_tca9535 failed");
    ESP_GOTO_ON_ERROR(init_spi(), cleanup, TAG, "init_spi failed");
    ESP_GOTO_ON_ERROR(init_adc(), cleanup, TAG, "init_adc failed");

    s_bsp.initialized = true;

    /* Стартовые уставки цифровых потенциометров (до включения силовых шин V1/V2/eLOAD) */
    ESP_GOTO_ON_ERROR(levisw_bsp_pot_set_wiper(LEVISW_BUS_V1, (uint8_t)CONFIG_LEVISW_DEFAULT_V1_WIPER),
                      cleanup, TAG, "Ошибка установки уставки DA2 (+V1)");
    ESP_GOTO_ON_ERROR(levisw_bsp_pot_set_wiper(LEVISW_BUS_V2, (uint8_t)CONFIG_LEVISW_DEFAULT_V2_WIPER),
                      cleanup, TAG, "Ошибка установки уставки DA3 (+V2)");
    ESP_GOTO_ON_ERROR(levisw_bsp_pot_set_wiper(LEVISW_BUS_ULOAD, (uint8_t)CONFIG_LEVISW_DEFAULT_ULOAD_WIPER),
                      cleanup, TAG, "Ошибка установки уставки DA4 (Uload)");

    /* Калибровка измерителей мощности DD5 (+V1) и DD6 (+V2) */
    ESP_GOTO_ON_ERROR(levisw_bsp_ina226_init(LEVISW_INA_V1), cleanup, TAG, "Ошибка инициализации DD5 INA226");
    ESP_GOTO_ON_ERROR(levisw_bsp_ina226_init(LEVISW_INA_V2), cleanup, TAG, "Ошибка инициализации DD6 INA226");

    ESP_LOGI(TAG, "BSP LeviSW-MB v1.2 инициализирован (I2C=%d Гц, V1_w=%d, V2_w=%d)",
             CONFIG_LEVISW_I2C_FREQ_HZ, CONFIG_LEVISW_DEFAULT_V1_WIPER, CONFIG_LEVISW_DEFAULT_V2_WIPER);
    return ESP_OK;

cleanup:
    s_bsp.initialized = false;
    levisw_bsp_deinit();
    return ret;
}

esp_err_t levisw_bsp_deinit(void)
{
    if (s_bsp.handles.i2c_tca9535 != NULL) {
        (void)i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P0, 0x00);
        (void)i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P1, 0x00);
    }

    gpio_set_level(LEVISW_PIN_SPI_EN2_DRIVERS, 0);

    if (s_bsp.adc1_cali != NULL) {
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        (void)adc_cali_delete_scheme_line_fitting(s_bsp.adc1_cali);
#endif
        s_bsp.adc1_cali = NULL;
    }

    if (s_bsp.handles.adc1_handle != NULL) {
        (void)adc_oneshot_del_unit(s_bsp.handles.adc1_handle);
        s_bsp.handles.adc1_handle = NULL;
    }

    if (s_bsp.handles.spi_tle94112 != NULL) {
        (void)spi_bus_remove_device(s_bsp.handles.spi_tle94112);
        s_bsp.handles.spi_tle94112 = NULL;
    }
    if (s_bsp.handles.spi_mc33879 != NULL) {
        (void)spi_bus_remove_device(s_bsp.handles.spi_mc33879);
        s_bsp.handles.spi_mc33879 = NULL;
    }
    if (s_bsp.spi_bus_init) {
        (void)spi_bus_free(SPI2_HOST);
        s_bsp.spi_bus_init = false;
    }

    i2c_master_dev_handle_t *devs[] = {
        &s_bsp.handles.i2c_tca9535,
        &s_bsp.handles.i2c_pot_v1,
        &s_bsp.handles.i2c_pot_v2,
        &s_bsp.handles.i2c_pot_uload,
        &s_bsp.handles.i2c_ina_v1,
        &s_bsp.handles.i2c_ina_v2,
    };
    for (size_t i = 0; i < sizeof(devs) / sizeof(devs[0]); i++) {
        if (*devs[i] != NULL) {
            (void)i2c_master_bus_rm_device(*devs[i]);
            *devs[i] = NULL;
        }
    }

    if (s_bsp.handles.i2c_bus != NULL) {
        (void)i2c_del_master_bus(s_bsp.handles.i2c_bus);
        s_bsp.handles.i2c_bus = NULL;
    }

    if (s_bsp.exp_mutex != NULL) {
        vSemaphoreDelete(s_bsp.exp_mutex);
        s_bsp.exp_mutex = NULL;
    }

    memset(&s_bsp, 0, sizeof(s_bsp));
    return ESP_OK;
}

const levisw_bsp_handles_t *levisw_bsp_get_handles(void)
{
    return s_bsp.initialized ? &s_bsp.handles : NULL;
}

/* --------------------------------------------------------------------------
 * Расширитель портов DD3 TCA9535PWR
 * ------------------------------------------------------------------------ */

esp_err_t levisw_bsp_exp_read_inputs(uint8_t *port0, uint8_t *port1)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    if (port0 != NULL) {
        ESP_RETURN_ON_ERROR(i2c_read_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_INPUT_P0, port0),
                            TAG, "Ошибка чтения INPUT_P0");
    }
    if (port1 != NULL) {
        ESP_RETURN_ON_ERROR(i2c_read_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_INPUT_P1, port1),
                            TAG, "Ошибка чтения INPUT_P1");
    }
    return ESP_OK;
}

esp_err_t levisw_bsp_exp_write_outputs(uint8_t port0, uint8_t port1)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    xSemaphoreTake(s_bsp.exp_mutex, portMAX_DELAY);

    uint8_t p0_masked = port0 & (uint8_t)(~LEVISW_EXP_P0_DIR_MASK);
    uint8_t p1_masked = port1 & (uint8_t)(~LEVISW_EXP_P1_DIR_MASK);

    esp_err_t err = i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P0, p0_masked);
    if (err == ESP_OK) {
        s_bsp.exp_out_p0 = p0_masked;
        err = i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P1, p1_masked);
        if (err == ESP_OK) {
            s_bsp.exp_out_p1 = p1_masked;
        }
    }

    xSemaphoreGive(s_bsp.exp_mutex);
    return err;
}

esp_err_t levisw_bsp_exp_update_bits(uint8_t p0_mask, uint8_t p0_val,
                                     uint8_t p1_mask, uint8_t p1_val)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    xSemaphoreTake(s_bsp.exp_mutex, portMAX_DELAY);

    uint8_t eff_p0_mask = p0_mask & (uint8_t)(~LEVISW_EXP_P0_DIR_MASK);
    uint8_t eff_p1_mask = p1_mask & (uint8_t)(~LEVISW_EXP_P1_DIR_MASK);

    uint8_t new_p0 = (s_bsp.exp_out_p0 & (uint8_t)(~eff_p0_mask)) | (p0_val & eff_p0_mask);
    uint8_t new_p1 = (s_bsp.exp_out_p1 & (uint8_t)(~eff_p1_mask)) | (p1_val & eff_p1_mask);

    esp_err_t err = ESP_OK;
    if (eff_p0_mask != 0U) {
        err = i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P0, new_p0);
        if (err == ESP_OK) {
            s_bsp.exp_out_p0 = new_p0;
        }
    }
    if (err == ESP_OK && eff_p1_mask != 0U) {
        err = i2c_write_reg8(s_bsp.handles.i2c_tca9535, TCA9535_REG_OUTPUT_P1, new_p1);
        if (err == ESP_OK) {
            s_bsp.exp_out_p1 = new_p1;
        }
    }

    xSemaphoreGive(s_bsp.exp_mutex);
    return err;
}

/* --------------------------------------------------------------------------
 * Питание, потенциометры MCP4532 и телеметрия INA226
 * ------------------------------------------------------------------------ */

esp_err_t levisw_bsp_power_set(bool v1_en, bool v2_en, bool vamp_on, bool eload_en)
{
    uint8_t mask = LEVISW_EXP_P0_V1_EN |
                   LEVISW_EXP_P0_V2_EN |
                   LEVISW_EXP_P0_VAMP_ON |
                   LEVISW_EXP_P0_ELOAD_EN;
    uint8_t val = 0;

    if (v1_en) {
        val |= LEVISW_EXP_P0_V1_EN;
    }
    if (v2_en) {
        val |= LEVISW_EXP_P0_V2_EN;
    }
    if (vamp_on) {
        val |= LEVISW_EXP_P0_VAMP_ON;
    }
    if (eload_en) {
        val |= LEVISW_EXP_P0_ELOAD_EN;
    }

    return levisw_bsp_exp_update_bits(mask, val, 0, 0);
}

esp_err_t levisw_bsp_power_good(bool *pg)
{
    ESP_RETURN_ON_FALSE(pg != NULL, ESP_ERR_INVALID_ARG, TAG, "pg == NULL");
    uint8_t p0 = 0;
    ESP_RETURN_ON_ERROR(levisw_bsp_exp_read_inputs(&p0, NULL), TAG, "Ошибка чтения PG");
    *pg = (p0 & LEVISW_EXP_P0_PGOOD) != 0U;
    return ESP_OK;
}

esp_err_t levisw_bsp_pot_set_wiper(levisw_power_bus_t bus, uint8_t wiper)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    ESP_RETURN_ON_FALSE(wiper <= MCP4532_MAX_WIPER, ESP_ERR_INVALID_ARG,
                        TAG, "Wiper MCP4532 должен быть 0..128");

    i2c_master_dev_handle_t dev = pot_handle_from_bus(bus);
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "Неверный канал потенциометра");

    uint8_t tx[2] = {
        MCP4532_CMD_WRITE_W0(0),
        wiper,
    };
    return i2c_master_transmit(dev, tx, sizeof(tx), LEVISW_I2C_TIMEOUT_MS);
}

esp_err_t levisw_bsp_pot_get_wiper(levisw_power_bus_t bus, uint8_t *wiper)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    ESP_RETURN_ON_FALSE(wiper != NULL, ESP_ERR_INVALID_ARG, TAG, "wiper == NULL");

    i2c_master_dev_handle_t dev = pot_handle_from_bus(bus);
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "Неверный канал потенциометра");

    uint8_t cmd = MCP4532_CMD_READ_W0;
    uint8_t rx[2] = {0};
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(dev, &cmd, 1, rx, sizeof(rx), LEVISW_I2C_TIMEOUT_MS),
                        TAG, "Ошибка чтения MCP4532");

    uint16_t raw = (((uint16_t)rx[0] & 0x01U) << 8) | (uint16_t)rx[1];
    *wiper = (raw > MCP4532_MAX_WIPER) ? MCP4532_MAX_WIPER : (uint8_t)raw;
    return ESP_OK;
}

esp_err_t levisw_bsp_ina226_init(levisw_ina_channel_t ch)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    i2c_master_dev_handle_t dev = ina_handle_from_ch(ch);
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "Неверный канал INA226");

    ESP_RETURN_ON_ERROR(i2c_write_reg16_be(dev, INA226_REG_CONFIG, INA226_CONFIG_DEFAULT),
                        TAG, "Ошибка записи конфигурации INA226");
    ESP_RETURN_ON_ERROR(i2c_write_reg16_be(dev, INA226_REG_CALIBRATION, INA226_CAL_20MOHM_0_1MA),
                        TAG, "Ошибка записи калибровки INA226");
    return ESP_OK;
}

esp_err_t levisw_bsp_ina226_read(levisw_ina_channel_t ch,
                                 float *bus_voltage_v,
                                 float *shunt_voltage_mv,
                                 float *current_ma,
                                 float *power_mw)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    i2c_master_dev_handle_t dev = ina_handle_from_ch(ch);
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "Неверный канал INA226");

    uint16_t raw_u16 = 0;

    if (bus_voltage_v != NULL) {
        ESP_RETURN_ON_ERROR(i2c_read_reg16_be(dev, INA226_REG_BUS_V, &raw_u16),
                            TAG, "Ошибка чтения Bus Voltage INA226");
        *bus_voltage_v = (float)raw_u16 * INA226_BUS_LSB_V;
    }

    if (shunt_voltage_mv != NULL) {
        ESP_RETURN_ON_ERROR(i2c_read_reg16_be(dev, INA226_REG_SHUNT_V, &raw_u16),
                            TAG, "Ошибка чтения Shunt Voltage INA226");
        *shunt_voltage_mv = (float)((int16_t)raw_u16) * INA226_SHUNT_LSB_MV;
    }

    if (current_ma != NULL) {
        ESP_RETURN_ON_ERROR(i2c_read_reg16_be(dev, INA226_REG_CURRENT, &raw_u16),
                            TAG, "Ошибка чтения Current INA226");
        *current_ma = (float)((int16_t)raw_u16) * INA226_CURRENT_LSB_MA;
    }

    if (power_mw != NULL) {
        ESP_RETURN_ON_ERROR(i2c_read_reg16_be(dev, INA226_REG_POWER, &raw_u16),
                            TAG, "Ошибка чтения Power INA226");
        *power_mw = (float)raw_u16 * INA226_POWER_LSB_MW;
    }

    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Матрица 8x10, SPI-драйверы D1/D2 и аудиоусилитель XS2
 * ------------------------------------------------------------------------ */

esp_err_t levisw_bsp_adg1608_select(uint8_t dozen, bool enable)
{
    ESP_RETURN_ON_FALSE(dozen < LEVISW_MATRIX_DOZENS_COUNT, ESP_ERR_INVALID_ARG,
                        TAG, "Индекс десятка должен быть 0..7");

    uint8_t p0_mask = LEVISW_EXP_P0_SW_A0 | LEVISW_EXP_P0_SW_A1;
    uint8_t p1_mask = LEVISW_EXP_P1_SW_A2 | LEVISW_EXP_P1_SW_EN;

    uint8_t p0_val = 0;
    if ((dozen & 0x01U) != 0U) {
        p0_val |= LEVISW_EXP_P0_SW_A0;
    }
    if ((dozen & 0x02U) != 0U) {
        p0_val |= LEVISW_EXP_P0_SW_A1;
    }

    uint8_t p1_val = 0;
    if ((dozen & 0x04U) != 0U) {
        p1_val |= LEVISW_EXP_P1_SW_A2;
    }
    if (enable) {
        p1_val |= LEVISW_EXP_P1_SW_EN;
    }

    return levisw_bsp_exp_update_bits(p0_mask, p0_val, p1_mask, p1_val);
}

uint8_t levisw_bsp_unit_to_tle_hb(uint8_t unit)
{
    if (unit >= LEVISW_MATRIX_UNITS_COUNT) {
        return 0U;
    }
    return LEVISW_UNIT_TO_TLE_HB[unit];
}

esp_err_t levisw_bsp_drivers_enable(bool enable)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    return gpio_set_level(LEVISW_PIN_SPI_EN2_DRIVERS, enable ? 1 : 0);
}

static esp_err_t spi_xfer16(spi_device_handle_t dev, uint16_t tx_frame, uint16_t *rx_frame)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_STATE, TAG, "SPI устройство не создано");

    spi_transaction_t trans = {
        .flags     = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length    = 16,
        .rxlength  = 16,
        .tx_data   = {
            (uint8_t)((tx_frame >> 8) & 0xFFU),
            (uint8_t)(tx_frame & 0xFFU),
            0, 0
        },
    };

    ESP_RETURN_ON_ERROR(spi_device_transmit(dev, &trans), TAG, "Ошибка SPI обмена");

    if (rx_frame != NULL) {
        *rx_frame = ((uint16_t)trans.rx_data[0] << 8) | (uint16_t)trans.rx_data[1];
    }
    return ESP_OK;
}

esp_err_t levisw_bsp_spi_tle94112_xfer(uint16_t tx_frame, uint16_t *rx_frame)
{
    return spi_xfer16(s_bsp.handles.spi_tle94112, tx_frame, rx_frame);
}

esp_err_t levisw_bsp_spi_mc33879_xfer(uint16_t tx_frame, uint16_t *rx_frame)
{
    return spi_xfer16(s_bsp.handles.spi_mc33879, tx_frame, rx_frame);
}

esp_err_t levisw_bsp_amp_set(bool amp1, bool amp2, bool amp3,
                               bool rel1, bool rel2, bool led_amp)
{
    uint8_t p0_mask = LEVISW_EXP_P0_LEDAMP;
    uint8_t p0_val  = led_amp ? LEVISW_EXP_P0_LEDAMP : 0U;

    uint8_t p1_mask = LEVISW_EXP_P1_ENA1_AMP1 |
                      LEVISW_EXP_P1_ENA2_AMP2 |
                      LEVISW_EXP_P1_ENA3_AMP3 |
                      LEVISW_EXP_P1_ENA4_REL1 |
                      LEVISW_EXP_P1_ENA5_REL2;
    uint8_t p1_val = 0U;

    if (amp1) {
        p1_val |= LEVISW_EXP_P1_ENA1_AMP1;
    }
    if (amp2) {
        p1_val |= LEVISW_EXP_P1_ENA2_AMP2;
    }
    if (amp3) {
        p1_val |= LEVISW_EXP_P1_ENA3_AMP3;
    }
    if (rel1) {
        p1_val |= LEVISW_EXP_P1_ENA4_REL1;
    }
    if (rel2) {
        p1_val |= LEVISW_EXP_P1_ENA5_REL2;
    }

    return levisw_bsp_exp_update_bits(p0_mask, p0_val, p1_mask, p1_val);
}

esp_err_t levisw_bsp_amp_alarm(bool *alarm_active)
{
    ESP_RETURN_ON_FALSE(alarm_active != NULL, ESP_ERR_INVALID_ARG, TAG, "alarm_active == NULL");
    uint8_t p1 = 0;
    ESP_RETURN_ON_ERROR(levisw_bsp_exp_read_inputs(NULL, &p1), TAG, "Ошибка чтения ALARMIN");
    *alarm_active = (p1 & LEVISW_EXP_P1_ALARMIN) != 0U;
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Детекция линий трубок (LU1/LU2), АЦП VAmp и интерфейс RS-485
 * ------------------------------------------------------------------------ */

bool levisw_bsp_lu1_read(void)
{
    return gpio_get_level(LEVISW_PIN_LU1_DO1) != 0;
}

bool levisw_bsp_lu2_read(void)
{
    return gpio_get_level(LEVISW_PIN_LU2_DO2) != 0;
}

esp_err_t levisw_bsp_vamp_read_mv(uint32_t *vamp_mv)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    ESP_RETURN_ON_FALSE(vamp_mv != NULL, ESP_ERR_INVALID_ARG, TAG, "vamp_mv == NULL");

    int raw = 0;
    ESP_RETURN_ON_ERROR(adc_oneshot_read(s_bsp.handles.adc1_handle, LEVISW_ADC_CH_VAMP, &raw),
                        TAG, "Ошибка чтения ADC1_CH6 (VAmp)");

    int pin_mv = 0;
    if (s_bsp.adc1_cali != NULL) {
        ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(s_bsp.adc1_cali, raw, &pin_mv),
                            TAG, "Ошибка калибровочного расчёта ADC");
    } else {
        pin_mv = (raw * 3300) / 4095;
    }

    *vamp_mv = (uint32_t)((float)pin_mv * LEVISW_VAMP_DIV_RATIO + 0.5f);
    return ESP_OK;
}

esp_err_t levisw_bsp_rs485_set_active(bool active)
{
    ESP_RETURN_ON_FALSE(s_bsp.initialized, ESP_ERR_INVALID_STATE, TAG, "BSP не инициализирован");
    return gpio_set_level(LEVISW_PIN_RS_ON, active ? 1 : 0);
}
