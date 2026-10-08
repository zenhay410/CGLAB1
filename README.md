# Лабораторная работа №1 — Основы 3D-графики

Le Son Luong, М8О-309БВ-24. №13 в списке, вариант 1 — куб.
В задании 12 вариантов: ((13 - 1) % 12) + 1 = 1.

## Объём работы

Один куб и первые пять дополнительных заданий:
1. Перспективная и ортографическая проекции переключаются в интерфейсе.
2. Перенос, вращение X/Y/Z и неравномерный масштаб X/Y/Z.
3. Движение по неплоской траектории и вращение. Есть запуск/пауза, скорость,
   радиус, вертикальная амплитуда, скорость вращения и перезапуск.
4. Изменение оттенка через ColorEdit3 (Tint).
5. Процедурный цвет вершин по локальным координатам, умноженный на Tint.

Шестое дополнительное задание исключено: в сцене один объект, один UBO
и один набор дескрипторов сцены. Дескрипторы ImGui обслуживают только интерфейс.

## Запуск

Распакуйте архив и запустите Lab1.exe. Сохраните папку shaders рядом с программой.
Нужен драйвер видеокарты с поддержкой Vulkan. SDK нужен для сборки, готовой
программе достаточно Vulkan runtime из драйвера. MinGW runtime включён статически.

## Управление

- F1 — перспектива и цвета вершин.
- F2 — ортографическая проекция, растянутый однотонный куб.
- F3 — один куб с настроенной анимацией. Включите Play / Pause для движения.
- Space — пауза/продолжение, если клавиатура не захвачена интерфейсом.
- Position / Rotation XYZ / Scale XYZ — преобразования куба.
- Tint — выбранный цвет. Local vertex colors — процедурная окраска вершин.
- Animate cube — участие куба в анимации, Play / Pause — запуск/пауза.
- Speed / Radius / Vertical amplitude / Spin — параметры анимации.
- Restart animation — сброс локального времени.

Кнопки демонстраций заменяют параметры сцены пресетом. Пауза замораживает время,
поэтому продолжение не вызывает скачка. Нижние элементы доступны прокруткой панели.

## Сборка

C++20, Vulkan, GLFW и Dear ImGui. Нужны CMake >= 3.20, Git, компилятор C++20,
Vulkan SDK и glslc в PATH. Первая конфигурация требует доступа в Интернет.

Windows с MinGW и Ninja:
```powershell
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
ctest --test-dir build-debug --output-on-failure
./build-debug/vulkan-starter-app.exe
```
Последняя команда запускается из корня проекта, где находится папка shaders.

Windows с Visual Studio:
```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Debug --parallel
ctest --test-dir build-msvc -C Debug --output-on-failure
./build-msvc/Debug/vulkan-starter-app.exe
```
Для VS 2026 замените генератор на Visual Studio 18 2026. Если preview-версия
не найдена CMake, используйте Developer Command Prompt и генератор Ninja.

Linux: те же команды Ninja, запуск ./build-debug/vulkan-starter-app.
Также нужны заголовки/загрузчик Vulkan, glslc, validation layers и зависимости
GLFW для X11/Wayland. Linux отдельно не проверялся.

Зависимости закреплены: GLFW 3.5.1, vk-bootstrap 1.4.321, VMA 3.4.0,
Dear ImGui 1.92.9, GLM 1.0.1.
Стартовый код: https://github.com/vladeemerr/vulkan-starter-app
Исходная ревизия: 7049a07b14f3f0deae7c9c0d3754bee88f876df0.
Лицензия основы сохранена в LICENSE и NOTICE.
Для новой копии репозитория upstream подключается так:
```bash
git remote add upstream https://github.com/vladeemerr/vulkan-starter-app
git fetch upstream
```
В рабочей копии этой лабораторной upstream уже добавлен.

## Файлы

source/application.cpp — ресурсы, интерфейс, анимация и команды рисования.
source/scene_math.hpp — куб, матрицы и траектория.
shaders/cube.vert и cube.frag — GLSL.
source/graphics_internal.cpp — основа преподавателя с исправлением пула ImGui.
tests/math_tests.cpp — проверки сетки, проекций, преобразований и траектории.
report — отчёт в PDF и снимки программы. DEFENSE.md — подготовка к защите.
