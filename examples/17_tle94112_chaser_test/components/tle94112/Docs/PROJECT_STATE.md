# 📋 Состояние проекта ESPUMAN (12 февраля 2026 г.)

## ✅ Статус: УСПЕШНО СОБРАН И РАБОТАЕТ

**Бинарный файл:** `build/ESPUMAN.bin` (238.3 KB)  
**Последняя сборка:** ✓ УСПЕШНО (0 ошибок, 1 предупреждение)  
**Последняя прошивка:** ✓ COM7 (UART)

---

## 🎯 Последние исправления/добавления

| Компонент | Статус | Описание |
|---|---|---|
| `ets_delay_us` → `esp_rom_delay_us` | ✅ DONE | Функция задержки в ESP-IDF 5.5.1 |
| I2C конфликт (driver_ng vs old API) | ✅ RESOLVED | MCP4532 использует старый I2C API |
| MCP4532 (2 устройства) | ✅ INTEGRATED | Адреса: 0x2C, 0x2D; функции: `set_digipot()` |
| TLE94112 диагностика | ✅ COMPLETED | Функция `tle94112_diagnostic_test()` |

---

## 🛠️ Архитектура компонентов

### ESP32 Периферия
- **I2C:** GPIO 32(SCL), 33(SDA), 100кHz, **старый API**
- **SPI:** GPIO 5(SCLK), 7(MOSI), 9(MISO), GPIO 10(CS TLE), GPIO 20(EN), 1MHz

### Устройства
| Имя | Тип | I2C Addr | SPI | Файлы |
|---|---|---|---|---|
| **TLE94112** | H-Bridge Driver (12 вых) | - | ✓ | `components/tle94112/` + `main/main.c` |
| **MCP4532** | Digital Pot. (2 шт) | 0x2C, 0x2D | - | `main/mcp4532.c(.h)` |
| **TCA95x5** | Расширитель GPIO | 0x20 | - | `components/tca95x5/` |
| **MC33879A** | Драйвер верхних ключей (High-Side) | - | ✓ | `components/mc33879a/` |

---

## 🚀 Инициализация (app_main)

```
1. I2C init + сканирование → MCP4532 init (set_digipot_all(0))
2. GPIO init (CS, EN пины)
3. SPI init (1MHz, LSB-first)
4. TLE94112 init (конфиг структура)
5. EN = HIGH, delay(500ms)
6. tle94112_diagnostic_test() → тест всех 12 выходов
7. tle94112_dump_act_cached() → финальный отчёт
```

**Время старта:** ~20 сек (диагностика 12 вых × 1.2s + инициализация)

---

## 📊 Диагностический тест TLE94112

**Функция:** `tle94112_diagnostic_test()` в `main/main.c`  
**Что проверяет:**
- Каждый из 12 выходов (HB1-HB12) включается на 1 сек в режиме HS_ON
- Читается статус диагностики (SYS_DIAG_1)
- Проверяются ошибки: OT, UV, OV, OL, LS, GS
- Вывод логов с символами (✓ OK, ⚠ ERROR)

**Результат:** Подтверждение работоспособности всех драйверов + вывод кешированных ACT регистров

---

## ⚙️ Конфигурация TLE94112

```c
// В main/main.c (глобально):
static tle94112_cfg_t tle94112_config = {
    .spi_host = SPI2_HOST,
    .gpio_mosi = 7,
    .gpio_miso = 9,
    .gpio_sclk = 5,
    .gpio_cs = 10,
    .gpio_en = 20,
};

// Инициализация:
 tle94112_init(&tle94112_config);

// Управление выходом:
 tle94112_set_hb_state(TLE_HB1, TLE_HB_HS_ON);   // ON
 tle94112_set_hb_state(TLE_HB1, TLE_HB_FLOATING); // OFF

// Диагностика:
 tle94112_read_sys_diag(&status);
 tle94112_dump_act_cached();
```

---

## 🔗 Файлы проекта

### Главные
- `main/main.c` — точка входа + диагностика
- `main/mcp4532.c(.h)` — управление потенциометрами
- `components/tle94112/` — драйвер TLE94112
- `components/tca95x5/` — I/O expander
- `components/mc33879a/` — high-side driver

### Документация (актуальна после обновления)
- `Docs/ARCHITECTURE.md` — архитектура системы + MCP4532
- `Docs/CLASS_DIAGRAM.md` — классы + MCP4532_Driver + TLE94112_DiagnosticTest
- `Docs/SEQUENCE_INITIALIZATION.md` — 6 фаз инициализации + диагностический тест
- `Docs/SPI_PROTOCOL.md` — SPI протокол TLE94112 (актуален)
- `Docs/STATE_MACHINE.md` — машина состояний (актуальна)

---

## 🐛 Известные проблемы / TODO

- ❌ Монитор серийного порта: нужно переподключить ESP32 для просмотра логов
- ⏳ Мониторинг TCA9535: уже реализован в `tca9535_control_task()`

---

## 📝 Краткая справка команд

```bash
# Сборка
idf.py build

# Прошивка (с выбором портов, если нужно)
idf.py -p COM7 flash

# Мониторинг
idf.py monitor

# Сборка + Flash одновременно
idf.py -p COM7 build flash monitor
```

---

## 🔍 Быстрый поиск по коду

| Функция | Файл | Строка |
|---|---|---|
| `tle94112_diagnostic_test()` | main/main.c | ~355 |
| `tle94112_init_device()` | main/main.c | ~65 |
| `tle94112_config` | main/main.c | ~48 |
| `set_digipot_all()` | main/mcp4532.c | - |
| `app_main()` | main/main.c | ~467 |

---

## 📞 Контакт с проектом

**Язык:** C (ESP-IDF 5.5.1)  
**ОС:** FreeRTOS  
**Плата:** ESP32  
**Целевые микросхемы:** TLE94112EL, MCP4532, TCA95x5  

---

**Последнее обновление документов:** Feb 12, 2026, 21:XX UTC
