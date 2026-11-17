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
  - [`04_lu_detector/`](./examples/04_lu_detector/) — **Узел 04: Детекция статуса трубок LU1 / LU2**:
    - Опрос дискретных линий детекции трубок `LU1 (GPIO36, TP7)` и `LU2 (GPIO37, TP6)` через триггер Шмитта `DD1 (74LVC2G17GW)` и BSP;
    - Программная фильтрация дребезга контактов и визуализация статуса на `LED1 (WS2812C)`.
  - [`05_onewire_opendoor/`](./examples/05_onewire_opendoor/) — **Узел 05: Каскад 1-Wire и управления открытием двери**:
    - Работа с интерфейсом `1-Wire` (`1WRx=GPIO12/MTDI`, `1WTx=GPIO15/MTDO`) через сборку `RN12`;
    - Аппаратное отпирание замка на клемме `XP1:9 (Open/1W)` через перемычку `R1 (0 Ом)` и полумост `PO2` (`SW-OUT2`) драйвера `D2 TLE94112EL`.
  - [`06_mcu_esp32_rf/`](./examples/06_mcu_esp32_rf/) — **Узел 06: Центральный микроконтроллер ESP32 и RF-тракт**:
    - Инициализация и системная диагностика MCU `DD4 (ESP32-PICO-V3)`;
    - Проверка беспроводного RF-тракта и инициализация Wi-Fi в режиме STA.
  - [`07_i2c_expander_matrix/`](./examples/07_i2c_expander_matrix/) — **Узел 07: I2C-расширитель портов и матрица перемычек UART/1W**:
    - Управление 16-битным расширителем `DD3 TCA9535PWR` (`0x20`) по шине `I2C-L`;
    - Коммутация силовых шин, мультиплексора `ADG1608` и реле через порты `P0`/`P1`;
    - Мониторинг линий `PG`, `ALARMIN` и прерывания `INT0` (`GPIO35`).
  - [`08_vamp_version_debug/`](./examples/08_vamp_version_debug/) — **Узел 08: Датчики VAMPOK, VersionPCB и интерфейсы отладки**:
    - Измерение напряжения питания усилителя `VAmpM` через делитель на `GPIO34 / ADC1_CH6`;
    - Определение аппаратной версии PCB (`VersionPCB`) на `GPIO14 / ADC2_CH6`;
    - Работа с отладочными интерфейсами `UART0 (XP5)` и `JTAG (XP4)`.
  - [`09_dozens_matrix_adg1608/`](./examples/09_dozens_matrix_adg1608/) — **Узел 09: Коммутатор десятков и диодная защита линий D0–D7**:
    - Управление аналоговым мультиплексором десятков `DA1 ADG1608BR` (`D0..D7`, `XP2:1..8`, `TP27..TP34`);
    - Контроль коммутации строк матрицы десятков с диодной защитой линий.
  - [`10_mc33879_voffset/`](./examples/10_mc33879_voffset/) — **Узел 10: Узел защиты от «перевертышей» и смещения VOffset**:
    - Управление 8-канальным драйвером `D1 MC33879APEKR2` по шине `SPI2` (`CS2` на `GPIO8`, `EN-DE` на `GPIO20`);
    - Использование прямых входов управления `IN5 (GPIO27)` и `IN6 (GPIO21)`.
  - [`11_units_matrix_tle94112/`](./examples/11_units_matrix_tle94112/) — **Узел 11: Коммутатор единиц E0–E9 и фильтр возврата -LINE**:
    - Управление 12-полумостовым SPI-драйвером `D2 TLE94112EL` (`E0..E9`, `XP3:1..10`, `TP35..TP44`);
    - Использование нелинейной таблицы маппинга единиц в BSP (`levisw_bsp_unit_to_tle_hb`).

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
cd examples/11_units_matrix_tle94112
idf.py set-target esp32
idf.py build
idf.py flash monitor
```
