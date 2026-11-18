# TLE94112 Component — Changes and Fixes

**Дата обновления:** 6 марта 2026 г.  
**Версия компонента:** 1.0  
**ESP-IDF:** 5.5.2/5.5.3

---

## 🔴 Критические исправления

### 1. Порядок инициализации `s_inited` флага

**Файл:** `tle94112.c`  
**Функция:** `tle94112_init()`

#### Проблема
Функция `tle94112_verify_device_id()` вызывалась **до** установки флага `s_inited = true`, но внутри неё есть проверка:
```c
if (!s_inited) return ESP_ERR_INVALID_STATE;
```

Это приводило к ошибке при инициализации:
```
W (291) TLE94112: Failed to read device ID
W (291) TLE94112: Device ID verification failed: ESP_ERR_INVALID_STATE
```

#### Исправление
Переместил установку `s_inited = true` **перед** вызовом `tle94112_verify_device_id()`:

```c
// --- Установка флага инициализации ПЕРЕД проверкой DEV_ID ---
s_inited = true;                                ///< Установка флага инициализации

// --- Проверка связи ---
uint8_t sys = tle_readStatus_reg(SYS_DIAG1);
ESP_LOGI(TAG, "SYS_DIAG_1 = 0x%02X", sys);

// --- Проверка DEV_ID ---
ret = tle94112_verify_device_id();
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Device ID verification failed: %s", esp_err_to_name(ret));
}
```

#### Результат
✅ Device ID проверка теперь работает корректно  
✅ В логе будет: `Device ID verified: TLE94112EL (0x00)`

---

### 2. Очистка ошибок TSD после инициализации

**Файл:** `tle94112.c`  
**Контекст:** Инициализация компонента

#### Проблема
После включения питания TLE94112 бит TSD (Thermal Shutdown) может быть установлен из-за:
- Первого включения
- Перегрева при пайке
- Ложного срабатывания

Этот бит блокирует все выходы драйвера.

#### Исправление
Добавлена очистка ошибок сразу после инициализации в `example_04_task()` (main.c):

```c
ret = tle94112_init(&s_tle_cfg);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "TLE94112 init failed: %s", esp_err_to_name(ret));
    vTaskDelete(NULL);
    return;
}

// ✅ Очистка ошибок сразу после инициализации
ret = tle94112_clear_errors();
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to clear initial errors: %s", esp_err_to_name(ret));
}
```

#### Результат
✅ TSD бит очищается после инициализации  
✅ Выходы драйвера разблокированы  
✅ В логе: `SYS_DIAG_1 = 0x00` (без ошибок)

---

### 3. Исправление битов SYS_DIAG_1

**Файлы:** 
- `tle94112.c` — функция `sys_diag_bit_message()`
- `example/example_04_diagnostics.c` — функция `check_power_and_temperature()`

#### Проблема
В документации TLE94112 биты регистра SYS_DIAG_1 (0x1B) расположены так:

| Бит | Имя | Описание |
|-----|-----|----------|
| 7 | HW_ERROR | Hardware failure |
| 6 | TSD | Thermal Shutdown |
| 5 | SPI_ERROR | SPI communication error |
| 4 | NPOR | No Power-on Reset |
| 3 | VS_OV | Over Voltage |
| 2 | VS_UV | Under Voltage |
| 1 | LE | Latch Error |
| 0 | UNUSED | Reserved |

В коде биты 2 и 3 были перепутаны местами.

#### Исправление
Приведено в соответствие с документацией:

```c
// Бит 3: VS_OV (Over Voltage)
if (sys_diag & (1 << 3)) {
    ESP_LOGE(TAG, "ERROR: VS Over-Voltage detected!");
    has_error = true;
}

// Бит 2: VS_UV (Under Voltage)
if (sys_diag & (1 << 2)) {
    ESP_LOGW(TAG, "WARNING: VS Under-Voltage detected!");
    has_error = true;
}
```

#### Результат
✅ Диагностика отображает правильные ошибки  
✅ VS_OV и VS_UV больше не перепутаны

---

## 📋 Функции компонента

### Публичный API

| Функция | Описание |
|---------|----------|
| `tle94112_init()` | Инициализация SPI и GPIO, проверка DEV_ID |
| `tle94112_deinit()` | Деинициализация компонента |
| `tle94112_set_hb_state()` | Установка состояния полумоста (HS_ON/LS_ON/FLOATING/BRAKE) |
| `tle94112_set_all_hb_state()` | Установка состояния всех 12 полумостов |
| `tle94112_read_sys_diag()` | Чтение регистра SYS_DIAG_1 |
| `tle94112_read_status_snapshot()` | Чтение всех статусных регистров |
| `tle94112_clear_errors()` | Очистка битов ошибок |
| `tle94112_log_diagnostic_report()` | Вывод полного отчёта диагностики |
| `tle94112_log_ports_table()` | Вывод таблицы состояний портов HB1-HB12 |
| `tle94112_verify_device_id()` | Проверка DEV_ID чипа |
| `tle94112_is_initialized()` | Проверка флага инициализации |

---

## 🔧 Конфигурация

### SPI параметры
```c
.spi_host  = SPI2_HOST
.gpio_mosi = 7
.gpio_miso = 9
.gpio_sclk = 5
.gpio_cs   = 10
.gpio_en   = 20
```

### Параметры из sdkconfig
| Параметр | Значение | Описание |
|----------|----------|----------|
| `CONFIG_ESPUMAN_SPI_CLOCK_SPEED` | 1000000 (1 МГц) | Скорость SPI |
| `CONFIG_ESPUMAN_SPI_MODE` | 1 | Mode 1 (CPOL=0, CPHA=1) |
| `CONFIG_ESPUMAN_TLE94112_SPI_LSB_FIRST` | 1 | LSB-first порядок |
| `CONFIG_ESPUMAN_STARTUP_DELAY_MS` | 500 | Задержка после EN=1 |

---

## 📊 Адреса регистров (LSB-first)

### Управляющие регистры (Control)
| Регистр | Адрес | Назначение |
|---------|-------|------------|
| HB_ACT_1_CTRL | 0x03 | Состояние HB1-HB4 |
| HB_ACT_2_CTRL | 0x43 | Состояние HB5-HB8 |
| HB_ACT_3_CTRL | 0x23 | Состояние HB9-HB12 |
| HB_MODE_1_CTRL | 0x63 | Режим HB1-HB4 |
| HB_MODE_2_CTRL | 0x13 | Режим HB5-HB8 |
| HB_MODE_3_CTRL | 0x53 | Режим HB9-HB12 |
| PWM_CH_FREQ_CTRL | 0x33 | Частота PWM |
| CONFIG_CTRL | 0x67 | DEV_ID (чтение) |

### Статусные регистры (Status)
| Регистр | Адрес | Назначение |
|---------|-------|------------|
| SYS_DIAG1 | 0x1B | Системная диагностика |
| ERR1_OC_1_4 | 0x5B | Overcurrent HB1-4 |
| ERR2_OC_5_8 | 0x3B | Overcurrent HB5-8 |
| ERR3_OC_9_12 | 0x7B | Overcurrent HB9-12 |
| ERR4_OL_1_4 | 0x07 | Open Load HB1-4 |
| ERR5_OL_5_8 | 0x47 | Open Load HB5-8 |
| ERR6_OL_9_12 | 0x27 | Open Load HB9-12 |

---

## 🎯 Цели исправлений

1. **Корректная инициализация** — Device ID проверка работает
2. **Чистое состояние** — TSD бит очищается после старта
3. **Правильная диагностика** — биты SYS_DIAG_1 соответствуют документации
4. **Стабильная работа** — все 12 полумостов управляются корректно

---

## 📝 История изменений

| Дата | Изменение | Статус |
|------|-----------|--------|
| 06.03.2026 | Исправлен порядок `s_inited = true` | ✅ |
| 06.03.2026 | Добавлена очистка ошибок после инициализации | ✅ |
| 06.03.2026 | Исправлены биты SYS_DIAG_1 (VS_OV/VS_UV) | ✅ |
