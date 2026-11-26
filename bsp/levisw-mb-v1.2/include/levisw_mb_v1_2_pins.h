/**
 * @file levisw_mb_v1_2_pins.h
 * @brief Аппаратная карта выводов, шин, адресов I2C и топологии матрицы платы LeviSW-MB v1.2.
 *
 * Все значения строго соответствуют принципиальной схеме U210001-A E3 (LeviSW-MB_v1.2):
 * - MCU (DD4): ESP32-PICO-V3 (LGA-48)
 * - Расширитель GPIO (DD3): TCA9535PWR (I2C 0x20)
 * - Матрица десятков D0..D7 (DA1): ADG1608BR + защита от переполюсовки D1 (MC33879APEKR2)
 * - Матрица единиц E0..E9 (D2): TLE94112EL (12 полумостов, нестандартная разводка клемм XP3)
 * - Питание: TPS65261RHBR (D3) + LM22675MRX-5.0 (DP1, DP2) + TL431 (DV1)
 * - Регулировка и телеметрия: MCP4532T-104E/MS (DA2, DA3, DA4) + INA226AIDGSR (DD5, DD6; DD7 DNP)
 */

#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "soc/adc_channel.h"
#include "soc/dac_channel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 1. Микроконтроллер DD4 (ESP32-PICO-V3) — UART / Отладка / Прошивка
 * ========================================================================== */
#define LEVISW_PIN_U0TX             GPIO_NUM_1   /* XP5:4 Tx (TP105, TP116), RN7 (U0-RS485) DNP */
#define LEVISW_PIN_U0RX             GPIO_NUM_3   /* XP5:5 Rx (TP106, TP117), RN7 (U0-RS485) DNP */
#define LEVISW_PIN_BOOT             GPIO_NUM_0   /* XP5:3 Boot (TP104), pull-up RN16 10k */

/* UART1 -> RS-485 трансивер DD2 MAX13487EESA+ через установленную сборку RN8 (1k x 2) */
#define LEVISW_PIN_RS485_TXD        GPIO_NUM_2   /* U1TXD -> RN8 -> DI (DD2:4, TP52, TP85) */
#define LEVISW_PIN_RS485_RXD        GPIO_NUM_4   /* U1RXD <- RN8 <- RO (DD2:1, TP53, TP86) */

/*
 * IO27 (TP89) на осмотренном экземпляре платы:
 *  1) RS-ON -> R11 -> DD2 MAX13487 nSHDN;
 *  2) BUTTON -> R10 <- SW1 MODE;
 *  3) CORN13/RN13 не установлена: IN5 MC33879 не соединён с IO27.
 *
 * Не использовать GPIO27 как управляющий вход IN5 для этой аппаратной
 * конфигурации. Входы IN5/IN6 MC33879 физически не подключены к GPIO.
 */
#define LEVISW_PIN_RS_ON            GPIO_NUM_27  /* RS-ON -> DD2 MAX13487 nSHDN */
#define LEVISW_PIN_BUTTON_MODE      GPIO_NUM_27  /* Кнопка SW1 MODE через R10 */


/* 1-Wire / OpenDoor (через установленную сборку RN12 GPIO-1W, пины JTAG MTDI/MTDO) */
#define LEVISW_PIN_1W_RX            GPIO_NUM_12  /* MTDI / U2RXD <- RN12 <- 1WRx (TP57, TP90) */
#define LEVISW_PIN_1W_TX            GPIO_NUM_15  /* MTDO / U2TXD -> RN12 -> 1WTx (TP93) */
#define LEVISW_PIN_JTAG_MTCK        GPIO_NUM_13  /* XP4:4 JTCK (TP91, TP108) */
#define LEVISW_PIN_JTAG_MTMS        GPIO_NUM_14  /* XP4:2 JTMS (TP92, TP107) — также ADC1-6 VersionPCB */

/* ============================================================================
 * 2. Шина I2C-L и линии прерываний (Лист 4 MCU-ESP.SchDoc)
 * ========================================================================== */
#define LEVISW_PIN_I2C_SCL          GPIO_NUM_32  /* SCL0 (TP100), pull-up RN15 10k к +3.3V */
#define LEVISW_PIN_I2C_SDA          GPIO_NUM_33  /* SDA0 (TP101), pull-up RN15 10k к +3.3V */
#define LEVISW_PIN_INT0_TCA9535     GPIO_NUM_35  /* I35 (вход, TP95) <- DD3 TCA9535 INT, pull-up RN15 10k */
#define LEVISW_PIN_INT1_INA_MV1     GPIO_NUM_38  /* I38 (вход, TP98) <- DD5 INA226 Alert (+V1), pull-up RN15 */
#define LEVISW_PIN_INT2_INA_MV2     GPIO_NUM_39  /* I39 (вход, TP99) <- DD6 INA226 Alert (+V2) */

/* 7-битные адреса I2C устройств (с учётом установленных / DNP резисторов адресации) */
#define LEVISW_I2C_ADDR_TCA9535     0x20         /* DD3: A2=0 (R21), A1=0 (R15), A0=0 (R13) */
#define LEVISW_I2C_ADDR_POT_V1      0x2C         /* DA2 MCP4532 (+V1): A1=0 (R44), A0=0 (R46) */
#define LEVISW_I2C_ADDR_POT_V2      0x2D         /* DA3 MCP4532 (+V2): A1=0 (R55), A0=1 (R56) */
#define LEVISW_I2C_ADDR_POT_ULOAD   0x2F         /* DA4 MCP4532 (Uload): A1=1 (R65), A0=1 (R67) */
#define LEVISW_I2C_ADDR_INA_V1      0x40         /* DD5 INA226 (+V1, R51=20mOhm): A1=GND (R48), A0=GND (R50) */
#define LEVISW_I2C_ADDR_INA_V2      0x41         /* DD6 INA226 (+V2, R62=20mOhm): A1=GND (R59), A0=VS (R60) */
/* Примечание: DD7 INA226 в канале MV1e (шунт R73=20mOhm) на плате не устанавливается (DNP). */

/* ============================================================================
 * 3. Шина SPI (драйверы D1 MC33879APEKR2 и D2 TLE94112EL)
 * ========================================================================== */
#define LEVISW_PIN_SPI_MOSI         GPIO_NUM_7   /* SPID  (TP113) -> D1:17 DI, D2:5 SDI */
#define LEVISW_PIN_SPI_MISO         GPIO_NUM_9   /* SPIQ  (TP111) <- D1:32 DO, D2:7 SDO */
#define LEVISW_PIN_SPI_CLK          GPIO_NUM_5   /* SPICLK(TP118) -> D1:16 SCLK, D2:20 SCLK */
#define LEVISW_PIN_SPI_CS0_SPARE    GPIO_NUM_19  /* SPICS0 CS-P (TP119, резерв) */
#define LEVISW_PIN_SPI_CS1_TLE      GPIO_NUM_10  /* SPICS1 CS-E (TP112) -> D2:19 CSN, pull-up RN19 10k */
#define LEVISW_PIN_SPI_CS2_MC33     GPIO_NUM_8   /* SPICS2 CS-D (TP114) -> D1:18 CS,  pull-up RN19 10k */
#define LEVISW_PIN_SPI_EN0_SPARE    GPIO_NUM_22  /* GPIO22 EN-P (TP120, резерв) */
#define LEVISW_PIN_SPI_EN2_DRIVERS  GPIO_NUM_20  /* GPIO20 EN-DE (TP115) -> D1:15 EN & D2:8 EN, pull-down RN19 10k */
/* MC33879 IN6 (D1:14): NC на осмотренном экземпляре, CORN13/RN13 не установлена. */

/* ============================================================================
 * 4. Детекция линий трубок (LU1/LU2), АЦП, ЦАП и LED (Листы 2, 3, 4)
 * ========================================================================== */
#define LEVISW_PIN_LU1_DO1          GPIO_NUM_36  /* I36 (вход, TP96) <- DD1:6 1Y <- LU1 (XP1:7, TP7, TP45, TP50) */
#define LEVISW_PIN_LU2_DO2          GPIO_NUM_37  /* I37 (вход, TP97) <- DD1:4 2Y <- LU2 (XP1:6, TP6, TP46, TP51) */

#define LEVISW_PIN_VAMP_MON         GPIO_NUM_34  /* I34 / ADC1_CH6 (TP81, TP94): делитель R16=510k / R17=75k */
#define LEVISW_ADC_CH_VAMP          ADC_CHANNEL_6 /* ADC1 Channel 6 (GPIO34) */
#define LEVISW_VAMP_DIV_RATIO       (585.0f / 75.0f) /* (510k + 75k) / 75k = 7.8 */

#define LEVISW_PIN_PCB_VERSION      GPIO_NUM_14  /* IO14 / ADC2_CH6 (TP82, TP92): делитель R18=75k / R19=1k, R22=4.7k */
#define LEVISW_ADC_CH_VERSION       ADC_CHANNEL_6 /* ADC2 Channel 6 (GPIO14, общий с JTAG MTMS) */

#define LEVISW_PIN_DAC1_SOUND1      GPIO_NUM_25  /* DAC_1 (TP87) -> RN9 22R -> XS2:4 SOUND1 (TP10) */
#define LEVISW_PIN_DAC2_SOUND2      GPIO_NUM_26  /* DAC_2 (TP88) -> RN9 22R -> XS2:6 SOUND2 (TP11) */
#define LEVISW_DAC_CH_SOUND1        DAC_CHAN_0   /* GPIO25 */
#define LEVISW_DAC_CH_SOUND2        DAC_CHAN_1   /* GPIO26 */

#define LEVISW_PIN_LED_WS2812       GPIO_NUM_23  /* IO23 -> LED1 WS2812C DIN (TP20), выход LEDOUT -> TP19 */

/* ============================================================================
 * 5. Битовая карта расширителя DD3 TCA9535PWR (I2C 0x20, Лист 4)
 * ========================================================================== */
/* Port 0 (P0.0 .. P0.7, байт 0) */
#define LEVISW_EXP_P0_ELOAD_EN      (1U << 0)    /* P00 (pin 4):  CEN_L1 -> DP1 LM22675 EN (TP59), 1=ВКЛ */
#define LEVISW_EXP_P0_PGOOD         (1U << 1)    /* P01 (pin 5):  PG (вход) <- D3 TPS65261 PGOOD (TP60) */
#define LEVISW_EXP_P0_V2_EN         (1U << 2)    /* P02 (pin 6):  V2EN -> D3 TPS65261 EN2 (+V2, TP61), 1=ВКЛ */
#define LEVISW_EXP_P0_V1_EN         (1U << 3)    /* P03 (pin 7):  V1EN -> D3 TPS65261 EN1 (+V1, TP62), 1=ВКЛ */
#define LEVISW_EXP_P0_VAMP_ON       (1U << 4)    /* P04 (pin 8):  VAMPON -> VT4/VT5 (+Vamp, TP63, TP153), 1=ВКЛ */
#define LEVISW_EXP_P0_LEDAMP        (1U << 5)    /* P05 (pin 9):  LEDAMP -> XS2:1 (TP22, TP80) */
#define LEVISW_EXP_P0_SW_A0         (1U << 6)    /* P06 (pin 10): D8-A0 -> DA1 ADG1608 A0 (TP64, TP123) */
#define LEVISW_EXP_P0_SW_A1         (1U << 7)    /* P07 (pin 11): D8-A1 -> DA1 ADG1608 A1 (TP65, TP124) */

/* Port 1 (P1.0 .. P1.7, байт 1) */
#define LEVISW_EXP_P1_SW_A2         (1U << 0)    /* P10 (pin 13): D8-A2 -> DA1 ADG1608 A2 (TP66, TP125) */
#define LEVISW_EXP_P1_SW_EN         (1U << 1)    /* P11 (pin 14): D8-EN -> DA1 ADG1608 EN (TP67, TP126), 1=ВКЛ */
#define LEVISW_EXP_P1_ENA5_REL2     (1U << 2)    /* P12 (pin 15): ENA5 -> XS2:16 ENREL2 (TP16, TP79) */
#define LEVISW_EXP_P1_ENA4_REL1     (1U << 3)    /* P13 (pin 16): ENA4 -> XS2:14 ENREL1 (TP15, TP78) */
#define LEVISW_EXP_P1_ENA3_AMP3     (1U << 4)    /* P14 (pin 17): ENA3 -> XS2:12 ENAMP3 (TP14, TP74) */
#define LEVISW_EXP_P1_ALARMIN       (1U << 5)    /* P15 (pin 18): ALARMIN (вход) <- XS2:11 ALARM-IN (TP23, TP75) */
#define LEVISW_EXP_P1_ENA2_AMP2     (1U << 6)    /* P16 (pin 19): ENA2 -> XS2:10 ENAMP2 (TP13, TP76) */
#define LEVISW_EXP_P1_ENA1_AMP1     (1U << 7)    /* P17 (pin 20): ENA1 -> XS2:8  ENAMP1 (TP12, TP77) */

/* Маски направления TCA9535 (1 = вход, 0 = выход): входы только P0.1 (PG) и P1.5 (ALARMIN) */
#define LEVISW_EXP_P0_DIR_MASK      LEVISW_EXP_P0_PGOOD
#define LEVISW_EXP_P1_DIR_MASK      LEVISW_EXP_P1_ALARMIN

/* ============================================================================
 * 6. Параметры измерительных шунтов и цепей ОС питания (Листы 7, 8, 9, 12)
 * ========================================================================== */
#define LEVISW_SHUNT_V1_MOHM        20U          /* R51 = 20 мОм ±1% (DD5 INA226 0x40) */
#define LEVISW_SHUNT_V2_MOHM        20U          /* R62 = 20 мОм ±1% (DD6 INA226 0x41) */
#define LEVISW_SHUNT_ULOAD_MOHM     20U          /* R73 = 20 мОм ±1% (DD7 DNP) */
#define LEVISW_TPS65261_VFB_MV      600U         /* Опорное напряжение FB TPS65261 ~0.6 В */
#define LEVISW_MV_R_TOP_OHM         75000U       /* R41 (MV1), R52 (MV2), R63 (MV1e) */
#define LEVISW_MV_R_SER_OHM         4700U        /* R42 (MV1), R53 (MV2), R64 (MV1e) */

/* ============================================================================
 * 7. Маппинг матрицы 8x10 (Десятки D0..D7 и Единицы E0..E9)
 * ========================================================================== */
#define LEVISW_MATRIX_DOZENS_COUNT  8U           /* D0..D7 (клеммник XP2:1..8) */
#define LEVISW_MATRIX_UNITS_COUNT   10U          /* E0..E9 (клеммник XP3:1..10) */
#define LEVISW_MATRIX_MAX_FLATS     80U          /* 80 абонентов */

/* Индексы полумостов TLE94112EL (1..12 соответствуют PO1..PO12 на Листе 6 E10.SchDoc) */
typedef enum {
    LEVISW_TLE_HB_E0       = 1,   /* PO1  (pin 2)  -> XP3:1  (E0, TP35) */
    LEVISW_TLE_HB_SW_OUT2  = 2,   /* PO2  (pin 23) -> SW-OUT2 -> R1 (0R) -> OpenD */
    LEVISW_TLE_HB_E6       = 3,   /* PO3  (pin 14) -> XP3:7  (E6, TP41) */
    LEVISW_TLE_HB_E5       = 4,   /* PO4  (pin 11) -> XP3:6  (E5, TP40) */
    LEVISW_TLE_HB_E1       = 5,   /* PO5  (pin 3)  -> XP3:2  (E1, TP36) */
    LEVISW_TLE_HB_E4       = 6,   /* PO6  (pin 10) -> XP3:5  (E4, TP39) */
    LEVISW_TLE_HB_E2       = 7,   /* PO7  (pin 4)  -> XP3:3  (E2, TP37) */
    LEVISW_TLE_HB_E9       = 8,   /* PO8  (pin 22) -> XP3:10 (E9, TP44) */
    LEVISW_TLE_HB_E3       = 9,   /* PO9  (pin 9)  -> XP3:4  (E3, TP38) */
    LEVISW_TLE_HB_E7       = 10,  /* PO10 (pin 15) -> XP3:8  (E7, TP42) */
    LEVISW_TLE_HB_E8       = 11,  /* PO11 (pin 17) -> XP3:9  (E8, TP43) */
    LEVISW_TLE_HB_SW_OUT12 = 12,  /* PO12 (pin 18) -> SW-OUT12 (TP152) */
} levisw_tle_hb_t;

/* Таблица перевода цифры единиц E[0..9] в номер полумоста TLE94112EL (1..12) */
static const uint8_t LEVISW_UNIT_TO_TLE_HB[LEVISW_MATRIX_UNITS_COUNT] = {
    LEVISW_TLE_HB_E0, /* E0 -> PO1  */
    LEVISW_TLE_HB_E1, /* E1 -> PO5  */
    LEVISW_TLE_HB_E2, /* E2 -> PO7  */
    LEVISW_TLE_HB_E3, /* E3 -> PO9  */
    LEVISW_TLE_HB_E4, /* E4 -> PO6  */
    LEVISW_TLE_HB_E5, /* E5 -> PO4  */
    LEVISW_TLE_HB_E6, /* E6 -> PO3  */
    LEVISW_TLE_HB_E7, /* E7 -> PO10 */
    LEVISW_TLE_HB_E8, /* E8 -> PO11 */
    LEVISW_TLE_HB_E9, /* E9 -> PO8  */
};

#ifdef __cplusplus
}
#endif
