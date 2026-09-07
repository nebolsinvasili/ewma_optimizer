# EWMA ARL Calculator

Инструмент для расчёта Average Run Length (ARL) двусторонних EWMA-контрольных карт на знаковой статистике (**SN**) и знаково-ранговой статистике (**SR**), а также подбора параметров `(λ, L)`, при которых ARL достигает целевого значения (по умолчанию 370). Предназначен для инженеров по качеству и аналитиков, настраивающих контрольные карты EWMA на требуемые свойства.

## Возможности

- Вычисляет ARL методом Монте-Карло для сетки параметров `λ × L` (по умолчанию 4×13 = 52 комбинации)
- Поддерживает два типа карт через флаг `--chart SN|SR`: знаковую (SN) и знаково-ранговую (SR)
- Ищет пару `(λ, L)`, дающую ARL, ближайший к целевому значению
- Параллелит вычисления по всем ядрам CPU (OpenMP)
- Возобновляет прерванный расчёт с контрольной точки
- Сохраняет результаты в CSV и выводит топ-N лучших пар
- Настраивается через JSON-конфиг или аргументы командной строки

## Описание проекта

EWMA-карта сглаживает статистику процесса экспоненциальным фильтром и сигнализирует при выходе за контрольные пределы. Проект реализует **двустороннюю** EWMA-карту; на каждой итерации берётся выборка размера `n` из стандартного нормального распределения и строится статистика подгруппы, после чего обновляется EWMA-статистика c константой сглаживания `λ`:

```
Z_t = λ·stat + (1 − λ)·Z_{t−1},   Z₀ = 0
```

Статистика подгруппы зависит от типа карты (`--chart`):

- **SN (знаковая)**: `stat = 2·t − n`, где `t` — число неотрицательных наблюдений. Учитывает только знаки наблюдений, диапазон `±n`.
- **SR (знаково-ранговая)**: `stat = Σ sgn(xᵢ)·rank(|xᵢ|)`, ранги — по абсолютному значению (средний ранг при связях). Учитывает знаки и величины наблюдений, диапазон `±n(n+1)/2`.

Сигнал происходит, когда `Z_t > UCL` или `Z_t < LCL` (строгие неравенства, как в SAS-программах книги). Используются стационарные контрольные пределы (Chakraborti & Graham, уравнения 4.17/4.22):

```
UCL = L · scale · √(λ/(2 − λ)),   LCL = −UCL
```

где `scale = √n` для SN и `scale = √(n(n+1)(2n+1)/6)` для SR.

Итоговый результат по каждой паре `(λ, L)` — средняя длина серии до сигнала (ARL). Программа перебирает сетку значений `λ` и `L`, запускает прогоны Монте-Карло и выбирает пару, чей ARL ближе всего к целевому (`target_ARL`).

## Требования

**Для C++ (основная программа):**

- Компилятор g++ с поддержкой C++17 (проверено на GCC 16)
- make
- OpenMP (входит в состав g++)
- Библиотека [nlohmann/json](https://github.com/nlohmann/json) (header-only)

**Для Python-биндингов (опционально):**

- Python ≥ 3.8
- pip

## Установка

### Зависимости

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install g++ make nlohmann-json3-dev
```

Arch Linux:

```bash
sudo pacman -S gcc make nlohmann-json
```

Windows (MSYS2 — рекомендуется):

```bash
# Установить MSYS2 с mingw64: https://www.msys2.org/
# В терминале MSYS2 MinGW 64-bit:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-nlohmann-json
```

Windows (vcpkg — альтернатива):

```powershell
# Установить vcpkg: https://aka.ms/vcpkg
vcpkg install nlohmann-json:x64-windows
# Затем собирать с флагом: -I "C:/vcpkg/installed/x64-windows/include"
```

### Сборка

Linux / macOS:

```bash
make
```

Windows (MSYS2 MinGW):

```bash
# В терминале MSYS2 MinGW 64-bit:
mingw32-make
# Или просто:
make
```

Исполняемый файл появится в `bin/ewma` (или `bin/ewma.exe` на Windows).

Сборка в режиме отладки:

```bash
make debug
```

## Настройка

Основные параметры задаются в `config/default_config.json`:

| Поле | Описание | Значение по умолчанию |
|------|----------|-----------------------|
| `simulations` | Число прогонов Монте-Карло на пару `(λ, L)` | `5000` |
| `n` | Размер подгруппы (число наблюдений на итерацию) | `14` |
| `max_iter` | Максимальная длина серии (обрыв, если сигнала нет) | `5000` |
| `target_ARL` | Целевой ARL | `370.0` |
| `n_cores` | Число ядер (0 или ≤0 = автоопределение) | `0` |
| `tolerance` | Ранняя остановка при отклонении ≤ tolerance (−1 = отключено) | `-1.0` |
| `top_n` | Сколько лучших пар выводить | `10` |
| `chart_type` | Тип карты: `SN` (знаковая) или `SR` (знаково-ранговая) | `SN` |
| `lambda_values`, `L_values` | Сетка параметров | `0.05 … 0.20` (шаг 0.05), `2.4 … 3.0` (шаг 0.05) |
| `resume` | Продолжить прерванный расчёт | `false` |

Полное описание схемы конфига и форматов файлов — в [docs/API.md](docs/API.md).

## Использование

### Запуск с конфигурацией по умолчанию

Linux / macOS:

```bash
./bin/ewma
```

Windows (MSYS2):

```bash
./bin/ewma.exe
```

Windows (cmd):

```cmd
bin\ewma.exe
```

Программа выводит конфигурацию, прогресс-бар и таблицу топ-10 пар `(λ, L)` с минимальным отклонением ARL от целевого. Файлы результатов создаются в текущей директории:

- `arl_results_final.csv` — все рассчитанные пары `lambda,L,ARL`
- `best_arl_pairs.csv` — топ-10 пар с отклонением от целевого ARL
- `arl_calculation.log` — журнал запусков

### Запуск знаково-ранговой карты (SR)

```bash
./bin/ewma --chart SR --simulations 1000 --lambda_start 0.05 0.20 0.05 --L_start 2.4 3.0 0.1
```

> **Рекомендация по выбору λ** (книга, разд. 3.2.3): λ = 0.05 для малых сдвигов, λ = 0.10 для умеренных, λ = 0.20 для больших. После выбора λ подбирается L так, чтобы достичь желаемого ARL_IC.

### Быстрый запуск (уменьшенная сетка)

```bash
./bin/ewma --simulations 1000 --lambda_start 0.10 0.10 --L_start 2.6 2.7 0.1
```

### Продолжение прерванного расчёта

```bash
./bin/ewma --resume
```

### Свой файл конфигурации

```bash
./bin/ewma --config my_config.json
```

### Справка по опциям

```bash
./bin/ewma --help
```

Пример вывода топ-пар:

```
=======================================================================
TOP 10 BEST PAIRS:
=======================================================================
  # |  lambda  |    L    |    ARL    | Deviation
---------------------------------------------------------------------
  1 |   0.100 |   2.700 |  500.20 |    0.20
  2 |   0.100 |   2.650 |  501.10 |    1.10
  ...
```

Альтернативно можно использовать make-цели: `make run`, `make run-config`, `make run-quick`, `make run-detailed`.

### Make-цели

| Цель | Действие |
|------|----------|
| `make` | Сборка |
| `make debug` | Отладочная сборка |
| `make run` | Сборка и запуск |
| `make run-config` | Запуск с `config/default_config.json` |
| `make run-quick` | Быстрый запуск (1000 прогонов) |
| `make run-detailed` | Подробный запуск (10000 прогонов) |
| `make validate` / `make test` | Валидация симулятора по книге Chakraborti & Graham (гл. 4.2.3) |
| `make clean-all` | Очистить результаты и сборку |
| `make lib` | Сборка shared library (`.so` / `.dll`) для Python-биндингов |

## Python-биндинги (библиотека)

Проект предоставляет Python-пакет `ewma`, использующий ctypes для вызова C++ ядра. Пакет можно использовать локально в проекте или устанавливать как библиотеку в других проектах.

### Установка Python-пакета

**Linux / macOS:**

```bash
# 1. Собрать shared library
make lib

# 2. Установить Python-пакет (режим разработки — симлинк, изменения в коде видны сразу)
pip install -e .

# Или скопировать .so в системную директорию и установить глобально:
cp lib/libewma.so /usr/local/lib/
pip install .
```

**Windows (MSYS2 / MinGW):**

```bash
# 1. Собрать shared library (в MSYS2 mingw64 терминале)
make lib

# 2. Установить Python-пакет
pip install -e .
```

При сборке через MinGW shared library будет `lib/libewma.dll` (или `libewma.dll.a` + `ewma-0.dll`). Python автоматически найдёт её через `ctypes.CDLL`.

**Windows (Visual Studio / MSVC):**

Для MSVC-инструментария потребуется CMake или ручная сборка DLL:

```powershell
# Пример через MSVC cl.exe (упрощённый)
cl /std:c++17 /EHsc /O2 /MD /LD /I include /I "C:/path/to/nlohmann" ^
    src/Calculator.cpp src/Config.cpp src/Simulator.cpp src/Utils.cpp ^
    src/ProgressBar.cpp src/bridge.cpp /Fe:libewma.dll /link /IMPLIB:libewma.lib

# Установить переменную окружения для Python
set EWMA_LIB_PATH=%CD%\libewma.dll
pip install -e .
```

> **Важно для Windows:** shared library (`.dll`) должна быть доступна Python при импорте. Если `pip install -e .` не находит DLL автоматически — установите переменную окружения `EWMA_LIB_PATH` на полный путь к `libewma.dll`.

### Установка в другой проект

```bash
# В каталоге проекта ewma-optimizer:
pip install -e .

# В другом проекте:
pip install /path/to/ewma-optimizer
# Или, если пакет опубликован:
# pip install ewma-optimizer
```

### Использование в Python

```python
from ewma import Ewma, Config, Result, Distribution

# --- Конфигурация ---
config = Config(
    simulations=5000,           # число прогонов Монте-Карло
    n=14,                       # размер подгруппы
    max_iter=5000,              # макс. длина серии
    target_ARL=370.0,           # целевой ARL
    n_cores=0,                  # 0 = автоопределение ядер
    chart_type="SN",            # "SN" или "SR"
    lambda_range=(0.05, 0.20, 0.05),  # (начало, конец, шаг)
    L_range=(2.4, 3.0, 0.05),   # (начало, конец, шаг)
    top_n=10,
    tolerance=-1.0,             # -1 = ранняя остановка отключена
)

ewma = Ewma(config)
```

**Расчёт ARL для одной пары (λ, L):**

```python
arl = ewma.calculate_arl(lambda_=0.10, L=2.667)
print(f"ARL = {arl:.2f}")  # например, ARL = 481.94
```

> `lambda` — зарезервированное слово Python, поэтому параметр назван `lambda_`.

**Расчёт ARL с полной статистикой распределения:**

```python
dist = ewma.calculate_arl_distribution(lambda_=0.10, L=2.667)
print(f"ARL={dist.ARL:.2f}, mean={dist.mean:.2f}, stddev={dist.stddev:.2f}")
print(f"Перцентили: p5={dist.p5:.0f}, p25={dist.p25:.0f}, p50={dist.p50:.0f}, "
      f"p75={dist.p75:.0f}, p95={dist.p95:.0f}")
print(f"Диапазон: [{dist.min:.0f}, {dist.max:.0f}]")
```

**Полный перебор сетки (λ, L):**

```python
results = ewma.run()  # возвращает List[Result]
print(f"Рассчитано {len(results)} пар")

for r in results[:5]:
    print(f"  lambda={r.lambda_:.3f}, L={r.L:.3f}, ARL={r.ARL:.2f}, dev={r.deviation:.2f}")
```

**Лучшая пара (ближайшая к target_ARL):**

```python
best = ewma.best_pair()
print(f"Лучшая пара: lambda={best.lambda_:.3f}, L={best.L:.3f}, ARL={best.ARL:.2f}")
```

**Загрузка конфига из JSON:**

```python
ewma = Ewma()
ewma.load_json('{"simulations": 1000, "n": 14, "chart_type": "SR"}')
```

### Тип карты SN vs SR

```python
# SN — знаковая статистика (по умолчанию)
arl_sn = ewma.calculate_arl(0.10, 2.667, chart_type="SN")

# SR — знаково-ранговая статистика
ewma_sr = Ewma(Config(simulations=1000, n=10, chart_type="SR"))
arl_sr = ewma_sr.calculate_arl(0.10, 2.794, chart_type="SR")
```

### Пример: подбор λ, L для заданного ARL

```python
from ewma import Ewma, Config

# Быстрый поиск (мало прогонов, грубая сетка)
config = Config(
    simulations=1000,
    n=14,
    target_ARL=370.0,
    lambda_range=(0.05, 0.20, 0.05),
    L_range=(2.4, 3.0, 0.05),
)
ewma = Ewma(config)
best = ewma.best_pair()
print(f"Приближение: lambda={best.lambda_:.3f}, L={best.L:.3f}, ARL={best.ARL:.2f}")

# Уточнение (узкая сетка вокруг найденного)
config2 = Config(
    simulations=5000,
    n=14,
    target_ARL=370.0,
    lambda_range=(best.lambda_ - 0.01, best.lambda_ + 0.01, 0.005),
    L_range=(best.L - 0.05, best.L + 0.05, 0.01),
)
ewma2 = Ewma(config2)
best2 = ewma2.best_pair()
print(f"Уточнение: lambda={best2.lambda_:.3f}, L={best2.L:.3f}, ARL={best2.ARL:.2f}")
```

### Make-цели для Python

| Цель | Действие |
|------|----------|
| `make lib` | Сборка shared library `lib/libewma.so` (Linux) / `libewma.dll` (Windows) |

## Валидация (`make validate`)

Тест `tests/validate` проверяет симулятор по книге Chakraborti & Graham,
*Nonparametric Statistical Process Control*, гл. 4.2.3 (EWMA-SN и EWMA-SR карты):

1. **Книжные дизайн-точки**: симулятор должен воспроизводить Monte-Carlo-значения
   книги (рис. 4.7/4.8, SAS-программы 5 и 6, 100 000 прогонов) с допуском ±5%:
   - **EWMA-SN** (n=1, λ=0.10, L=2.667) → ARL_IC ≈ 481.94 (номинал 500);
   - **EWMA-SR** (n=10, λ=0.10, L=2.794) → ARL_IC ≈ 484.34 (номинал 500).
2. **Номинал 500**: обе книжные точки отклоняются от 500 не более чем на 10%.
3. **Воспроизводимость**: два независимых прогона Монте-Карло (разные зерна)
   расходятся не более чем на ±2%.

Возвращает ненулевой код при любом провале (в отличие от основного бинарника,
который всегда завершается с кодом 0).

## Roadmap

Планируемые направления (на основе текущих пробелов в проекте — список предложений):

- **Выходные коды**: сейчас программа всегда завершается с кодом 0; ввести ненулевые коды при ошибках (не найден конфиг, не открывается файл)
- **Распределение длин серий**: функции `simulateBatchWithDistribution` уже есть в коде, но не подключены к CLI — добавить режим вывода полной статистики (процентили 5/25/50/75/95, stddev, минимум/максимум)
- **Точные пределы EWMA**: реализованы только стационарные пределы (4.17/4.22); времязависимые пределы `(1 − (1 − λ)^{2t})` (4.16/4.21) не реализованы
- **Утилизация** `ResultManager`: класс реализован, но не участвует в сборке (Makefile его не компилирует) — либо использовать, либо удалить
- **Визуализация**: скрипты построения контурных графиков ARL по сетке `(λ, L)`
- **Разные целевые ARL**: поддержка нескольких целей за один запуск

Готово (реализовано):

- **Python-биндинги**: ctypes-обёртка для вызова из Python, pip-установка, Windows-совместимость

## Структура проекта

```
config/         JSON-конфигурации (default_config.json)
include/        Заголовочные файлы (Config, Calculator, Simulator, Utils, ProgressBar, bridge.h)
src/            Исходники (main, Calculator, Simulator, Config, Utils, ProgressBar, bridge.cpp)
bin/            Собранный исполняемый файл (создаётся make)
build/          Объектные файлы (создаются make)
lib/            Shared library (создаётся make lib): libewma.so / .dll
python/ewma/    Python-пакет (ctypes-биндинги): __init__.py, _bindings.py, _utils.py
tests/          Тесты валидации по книге (tests/validate, tests/validate.cpp)
pyproject.toml  Конфигурация pip-пакета
```

## Лицензия

MIT (другая лицензия в проекте не указана).