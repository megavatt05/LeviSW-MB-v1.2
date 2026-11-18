# Компонент TLE94112 для ESP-IDF

Драйвер инфракрасного мостового драйвера **Infineon TLE94112EL** для фреймворка ESP-IDF.

## Возможности

- ✅ Управление 12-ю полумостами (HB1-HB12)
- ✅ Поддержка всех режимов работы (FLOATING, LS_ON, HS_ON, BRAKE)
- ✅ Device ID верификация (автоопределение чипа)
- ✅ Кеширование состояний регистров
- ✅ SPI Mode 1, LSB-first
- ✅ Полная диагностика ошибок (Overcurrent, Open Load)
- ✅ Потокобезопасность (mutex)

## Поддерживаемые чипы

| Device ID | Название | Полумостов |
|-----------|----------|------------|
| 0x0 | TLE94112EL | 12 |
| 0x1 | TLE94110EL | 10 |
| 0x2 | TLE94108EL | 8 |
| 0x3 | TLE94106EL | 6 |
| 0x4 | TLE94104EP | 4 |
| 0x5 | TLE94103EP | 3 |

## Аппаратное подключение

| Пин ESP32 | Пин TLE94112 | Назначение |
|-----------|--------------|------------|
| GPIO7 | MOSI (Pin 4) | Выход данных ESP32 → TLE94112 |
| GPIO9 | MISO (Pin 3) | Вход данных TLE94112 → ESP32 |
| GPIO5 | SCLK (Pin 2) | Тактовый сигнал SPI |
| GPIO10 | CS (Pin 1) | Выбор кристалла |
| GPIO20 | EN (Pin 18) | Включение питания |

**Параметры SPI:** Mode 1 (CPOL=0, CPHA=1), LSB-first, до 5 MHz.

---

## API Reference

### Инициализация

```c
#include "tle94112.h"

// Конфигурация SPI шины (выполняется один раз)
spi_bus_config_t buscfg = {
    .mosi_io_num = 7,
    .miso_io_num = 9,
    .sclk_io_num = 5,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
};
spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

// Конфигурация драйвера
tle94112_config_t cfg = {
    .spi_host    = SPI2_HOST,
    .gpio_cs     = 10,
    .gpio_en     = 20,
    .spi_freq_hz = 1000000,  // 1 MHz
};

tle94112_handle_t handle = tle94112_init(&cfg);
if (!handle) {
    ESP_LOGE(TAG, "TLE94112 init failed!");
    return;
}
```

### Управление полумостами

```c
// Установить состояние одного полумоста
esp_err_t tle94112_set_hb_state(handle, TLE_HB1, TLE_HB_LS_ON);

// Установить состояние всех полумостов одновременно
esp_err_t tle94112_set_all_hb_state(handle, TLE_HB_FLOATING);
```

### Состояния полумостов

| Состояние | Значение | Описание |
|-----------|----------|----------|
| `TLE_HB_FLOATING` | 0b00 | Высокое сопротивление (выключено) |
| `TLE_HB_LS_ON` | 0b01 | Low-Side включен (OUT → GND) |
| `TLE_HB_HS_ON` | 0b10 | High-Side включен (VS → OUT) |
| `TLE_HB_BRAKE` | 0b11 | Торможение (оба ключа замкнуты) |

### Диагностика

```c
// Базовая диагностика (SYS_DIAG_1)
tle94112_diagnose(handle);

// Полная диагностика с указанием конкретных HB
tle94112_full_diagnose(handle);

// Получить маску overcurrent (бит N = HB N+1)
uint16_t oc_mask = 0;
tle94112_get_overcurrent(handle, &oc_mask);
if (oc_mask & (1 << 0)) {
    ESP_LOGW(TAG, "HB1 has overcurrent!");
}

// Получить маску open load (бит N = HB N+1)
uint16_t ol_mask = 0;
tle94112_get_openload(handle, &ol_mask);

// Очистка флагов ошибок
tle94112_clear_errors(handle);
tle94112_clear_overcurrent(handle);
tle94112_clear_openload(handle);
```

### Деинициализация

```c
tle94112_deinit(handle);
```

---

## Карта регистров

| Регистр | RD | WR | Назначение |
|---------|----|----|------------|
| `HB_ACT_1_CTRL` | 0x03 | 0x83 | Управление HB1-4 |
| `HB_ACT_2_CTRL` | 0x43 | 0xC3 | Управление HB5-8 |
| `HB_ACT_3_CTRL` | 0x23 | 0xA3 | Управление HB9-12 |
| `HB_MODE_1_CTRL` | 0x63 | 0xE3 | PWM режим HB1-4 |
| `HB_MODE_2_CTRL` | 0x13 | 0x93 | PWM режим HB5-8 |
| `HB_MODE_3_CTRL` | 0x53 | 0xD3 | PWM режим HB9-12 |
| `PWM_CH_FREQ` | 0x33 | 0xB3 | Частота PWM |
| `PWM1_DC` | 0x73 | 0xF3 | Duty cycle PWM1 |
| `PWM2_DC` | 0x0B | 0x8B | Duty cycle PWM2 |
| `PWM3_DC` | 0x4B | 0xCB | Duty cycle PWM3 |
| `FW_OL_CTRL` | 0x2B | 0xAB | Free-wheeling / Open Load |
| `FW_CTRL` | 0x6B | 0xEB | Free-wheeling |
| `CONFIG_CTRL` | 0x67 | — | Device ID (read only) |
| `SYS_DIAG_1` | 0x1B | 0x9B | Глобальный статус |
| `SYS_DIAG_2` | 0x5B | 0xDB | Overcurrent HB1-4 |
| `SYS_DIAG_3` | 0x3B | 0xBB | Overcurrent HB5-8 |
| `SYS_DIAG_4` | 0x7B | 0xFB | Overcurrent HB9-12 |
| `SYS_DIAG_5` | 0x07 | 0x87 | Open Load HB1-4 |
| `SYS_DIAG_6` | 0x47 | 0xC7 | Open Load HB5-8 |
| `SYS_DIAG_7` | 0x27 | 0xA7 | Open Load HB9-12 |

---

## Формат SPI кадра

16 бит: `[Байт адреса] [Байт данных]`

**Байт адреса:**
- Бит 7 (OP): 0 = Read, 1 = Write
- Биты 6-2 (Addr): 5-битный адрес регистра
- Бит 1 (LABT): Last Address Bit Token = 1 (обязателен!)
- Бит 0: 1 (всегда)

**Формула:**
- Read: `(addr_5bit << 2) | 0x03`
- Write: `0x80 | (addr_5bit << 2) | 0x03`

---

## Пример использования

```c
#include "tle94112.h"

void app_main(void)
{
    // Инициализация SPI шины
    spi_bus_config_t buscfg = {
        .mosi_io_num = 7,
        .miso_io_num = 9,
        .sclk_io_num = 5,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // Инициализация драйвера
    tle94112_config_t cfg = {
        .spi_host    = SPI2_HOST,
        .gpio_cs     = 10,
        .gpio_en     = 20,
        .spi_freq_hz = 1000000,
    };
    
    tle94112_handle_t handle = tle94112_init(&cfg);
    if (!handle) {
        ESP_LOGE("APP", "TLE94112 init failed!");
        return;
    }

    // Включить мотор (H-bridge из HB1 и HB2)
    tle94112_set_hb_state(handle, TLE_HB1, TLE_HB_HS_ON);  // VS → OUT1
    tle94112_set_hb_state(handle, TLE_HB2, TLE_HB_LS_ON);  // OUT2 → GND
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Остановить мотор
    tle94112_set_hb_state(handle, TLE_HB1, TLE_HB_FLOATING);
    tle94112_set_hb_state(handle, TLE_HB2, TLE_HB_FLOATING);
    
    // Диагностика
    tle94112_full_diagnose(handle);
    
    tle94112_deinit(handle);
}
```

---

## Регистр CONFIG_CTRL (Device ID)

**Адрес:** 0x67 (только чтение)

Регистр используется для идентификации чипа.

| Биты | Поле | Значение | Описание |
|------|------|----------|----------|
| D7:D3 | reserved | 0 | Всегда читается как '0' |
| D2:D0 | DEV_ID | 0-5 | Идентификатор устройства |

**Таблица Device ID (согласно даташиту Infineon):**

| DEV_ID | CONFIG_CTRL | Чип | Полумостов |
|--------|-------------|-----|------------|
| 000 | **0x00** | **TLE94112EL** | 12 |
| 001 | 0x01 | TLE94110EL | 10 |
| 010 | 0x02 | TLE94108EL | 8 |
| 011 | 0x03 | TLE94106EL | 6 |
| 100 | 0x04 | TLE94104EP | 4 |
| 101 | 0x05 | TLE94103EP | 3 |

**Пример:** Если `CONFIG_CTRL = 0x00`, значит на плате установлен чип **TLE94112EL** (12 полумостов).

---

## Диагностика SYS_DIAG_1

| Бит | Флаг | Описание |
|-----|------|----------|
| 7 | SPI_ERR | Ошибка протокола SPI |
| 6 | LE | Load Error (OC или OL) |
| 5 | VS_UV | Пониженное напряжение |
| 4 | VS_OV | Повышенное напряжение |
| 3 | NPOR | Нет Power-On Reset |
| 2 | TSD | Thermal Shutdown (перегрев) |
| 1 | TPW | Температурное предупреждение |

---

## Известные ограничения

1. **Open Load детекция** рассчитана на моторы (~100мА+). LED-нагрузка может вызывать ложные срабатывания.
2. **PWM функции** ещё не реализованы в API (регистры определены).
3. **Free-wheeling** режимы не реализованы в API.

---

## Совместимость

- **ESP32**: Полная поддержка
- **ESP32-C3**: Полная поддержка
- **ESP32-S2/S3**: Полная поддержка

---

## Лицензия

MIT License

## Автор

Albert, 2026
