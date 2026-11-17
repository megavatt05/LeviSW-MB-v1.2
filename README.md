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

## Быстрый старт

Подключение компонента в корневом `CMakeLists.txt` прошивки:

```cmake
cmake_minimum_required(VERSION 3.16)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/levisw-mb-v1.2")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(levisw_firmware)
```
