# LeviSW-MB v1.2

Репозиторий проекта контроллера матричного домофонного коммутатора **LeviSW-MB v1.2** (матрица `8 × 10` на 80 абонентов, MCU `ESP32-PICO-V3`).

## Структура репозитория

- [`bsp/levisw-mb-v1.2/`](./bsp/levisw-mb-v1.2/) — Пакет поддержки платы (Board Support Package, BSP) для **ESP-IDF >= 5.2**:
  - `idf_component.yml` — манифест компонента ESP-IDF;
  - `CMakeLists.txt` — регистрация компонента и зависимостей (`driver`, `esp_adc`, `esp_timer`);
  - `Kconfig` — меню настроек частот I2C/SPI и стартовых уставок `MCP4532`;
  - `include/levisw_mb_v1_2_pins.h` — карта выводов MCU `DD4`, адресов I2C, масок `TCA9535` и топологии матрицы;
  - `include/levisw_mb_v1_2_bsp.h` — публичный C/C++ API;
  - `src/levisw_mb_v1_2_bsp.c` — драйвер безопасной инициализации, питания, телеметрии и обмена по SPI/I2C;
  - [`bsp/levisw-mb-v1.2/README.md`](./bsp/levisw-mb-v1.2/README.md) — подробная схемотехническая документация и контрольные точки.

- [`examples/`](./examples/) — Готовые примеры применения для аппаратных узлов платы:
  - [`01_terminals_and_indication/`](./examples/01_terminals_and_indication/) — **Узел 01: Входные/выходные клеммы и локальная индикация**:
    - Управление адресным светодиодом `LED1 (WS2812C, GPIO23, TP20)`;
    - Обработка кнопки сервисного режима `SW1 MODE (GPIO27, TP21)`;
    - Мониторинг клеммника `XP1:6/7` (линии трубок `LU1/LU2`);
    - Импульс отпирания двери на `XP1:9 (Open/1W)` через `R1 (0 Ом)` к выходу `PO2/SW-OUT2`;
    - Тестовый перебор клеммников десятков `XP2:1..8 (D0..D7)` и единиц `XP3:1..10 (E0..E9)`.
  - [`03_rs485_transceiver/`](./examples/03_rs485_transceiver/) — **Узел 03: Дифференциальный трансивер шины RS-485**:
    - Настройка UART1 (`GPIO2` TXD, `GPIO4` RXD) для работы с `DD2 (MAX13487EESA+)`;
    - Управление режимом Shutdown трансивера через `GPIO27 (RS-ON)` (`levisw_bsp_rs485_set_active`);
    - Организация циклического обмена пакетами по дифференциальной линии с защитой `FR1/FR2` и `FS1`.

## Быстрый старт

### 1. Подключение BSP в вашем проекте
В корневом `CMakeLists.txt` прошивки добавьте путь к BSP:

```cmake
cmake_minimum_required(VERSION 3.16)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/levisw-mb-v1.2")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(levisw_firmware)
```

### 2. Запуск примеров
```bash
cd examples/03_rs485_transceiver
idf.py set-target esp32
idf.py build
idf.py flash monitor
```
