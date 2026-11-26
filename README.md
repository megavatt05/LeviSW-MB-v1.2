# LeviSW-MB v1.2

Репозиторий проекта контроллера матричного домофонного коммутатора **LeviSW-MB v1.2** (матрица `8 × 10` на 80 абонентов, MCU `ESP32-PICO-V3`).

## Аппаратная документация и анализ схемы

Подробные материалы по принципиальной схеме, анализу аппаратных узлов и даташитам компонентов размещены в отдельной ветке:

**[docs/hardware-reference — аппаратная документация](https://github.com/megavatt05/LeviSW-MB-v1.2/tree/docs/hardware-reference/docs/hardware)**

В этой ветке находятся:
- [Анализ принципиальной схемы](https://github.com/megavatt05/LeviSW-MB-v1.2/blob/docs/hardware-reference/docs/hardware/SCHEMATIC_ANALYSIS.md)
- [Даташиты и официальные ссылки производителей](https://github.com/megavatt05/LeviSW-MB-v1.2/blob/docs/hardware-reference/docs/hardware/DATASHEETS.md)

> Примечание: ветка с документацией пока не объединена с `main`. Исходный PDF КД и локальные копии даташитов будут добавлены туда отдельными бинарными файлами после передачи файлов в GitHub.

## Структура репозитория

- [`bsp/levisw-mb-v1.2/`](./bsp/levisw-mb-v1.2/) — пакет поддержки платы (BSP) для **ESP-IDF >= 5.2**.
- [`examples/`](./examples/) — примеры применения аппаратных узлов платы.

## Быстрый старт

### Подключение BSP

В корневом `CMakeLists.txt` прошивки добавьте путь к BSP:

```cmake
cmake_minimum_required(VERSION 3.16)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/bsp/levisw-mb-v1.2")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(levisw_firmware)
```

### Запуск примеров

```bash
cd examples/13_tps65261_dcdc
idf.py set-target esp32
idf.py build
idf.py flash monitor
```
