# HW3D

HW3D читает движущиеся треугольники, находит пересечения и визуализирует
сцену. Серые треугольники ни с чем не пересекаются, красные пересекаются хотя
бы с одним другим треугольником. Касание вершиной или ребром считается
пересечением.

В проекте один backend: Vulkan 1.4. CPU-реализации пересечений и OpenGL-пути
нет.

## Архитектура

Оконный кадр целиком выполняется одним command buffer и одним queue submit:

~~~text
immutable InitialTriangle buffer
  → rotation compute (вершины + AABB + highlighted = 0)
  → compute barrier
  → float SAT intersections
  → vertex barrier
  → dynamic rendering
  → present
~~~

Rotation, intersections и vertex shader работают с одним 80-байтовым
device-local runtime buffer. Кадр не загружает и не скачивает runtime-
геометрию. Один кадр в полёте позволяет не дублировать runtime buffer.

Перед созданием Vulkan instance конфигурация проверяется на конечность,
ненулевую ось и безопасный для GPU float диапазон. double применяется
только на CPU при валидации, расчёте motion bounds и ведении времени.

Narrow phase использует строгий SAT без epsilon:

- 2 нормали граней;
- 9 попарных cross product рёбер;
- 6 осей normal×edge;
- нулевые оси не пропускаются и естественно дают интервал [0, 0];
- разделение строгое: maxA < minB || maxB < minA;
- вырожденный triangle разрешён во входе, но всегда получает
  highlighted = false и не подсвечивает обычный triangle.

Потенциальные пары один раз строятся по motion AABB. Обычно они хранятся в
одном device-local pair buffer, разделённом на выровненные descriptor ranges.
Если заданный бюджет исчерпан, используется статический voxel fallback с
обходом своей и 26 соседних ячеек.

## Требования

- Linux;
- CMake 3.28+ и Ninja;
- компилятор с C++23 modules;
- GLFW 3.3+;
- Vulkan loader и устройство с Vulkan 1.4;
- glslc;
- необязательно spirv-val — при наличии он автоматически проверяет shaders;
- Git и доступ к GitHub при первой конфигурации.

CMake получает официальные Vulkan-Headers v1.4.363 и GoogleTest v1.17.0
через FetchContent. OpenGL, GLAD и VMA не используются.

## Сборка и тесты

~~~bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
~~~

Для оптимизированной сборки:

~~~bash
cmake --preset release
cmake --build --preset release
ctest --preset release
~~~

Тесты можно отключить параметром -DBUILD_TESTING=OFF. Отдельные группы:

~~~bash
ctest --preset debug -L unit
ctest --preset debug -L e2e
~~~

Unit-тесты проверяют validator, motion bounds и pair/voxel candidates.
E2E-набор проверяет отклонение удалённых CLI-флагов, оконный Vulkan-сценарий
и generator→windowed Vulkan. Оконные тесты используют xvfb-run, если
графическая сессия отсутствует.

В Debug validation layer включается при наличии. Для сценария, где stderr
должен быть пустым, его можно отключить:

~~~bash
HW3D_DISABLE_VALIDATION=1 xvfb-run -a \
  ./build/debug/hw3d < triangles.txt
~~~

## Ввод

~~~text
N
duration_seconds
x11 y11 z11
x12 y12 z12
x13 y13 z13
axis1_point1_x axis1_point1_y axis1_point1_z
axis1_point2_x axis1_point2_y axis1_point2_z
angular_speed_1_degrees_per_second
...
~~~

Ограничения:

- 0 < N < 1 000 000;
- длительность — положительное целое число секунд;
- координаты, ось и скорость конечны;
- две точки оси различны;
- весь полный оборот и произведение speed × duration должны помещаться в
  валидируемую безопасную область float.

Положительная входная скорость задаёт вращение по часовой стрелке при взгляде
со второй точки оси на первую. Внутри знак скорости инвертируется, а скорость
хранится в радианах в секунду.

Генератор:

~~~bash
python3 generate_triangles.py 5.0 1000 100.0 \
  --duration 30 --angular-speed 90 --seed 42 > triangles.txt
~~~

## Запуск

~~~bash
./build/debug/hw3d < triangles.txt
~~~

Мышь вращает камеру, стрелки перемещают её, Esc закрывает окно. После
представления точного состояния в duration_seconds программа завершится сама.
CLI не принимает аргументы. Удалённые --headless и --use-gpu-intersections,
как и любой другой аргумент, завершаются кодом 1 до чтения конфигурации и
инициализации GLFW/Vulkan.

## Структура

~~~text
src/
├── main.cpp
├── configuration/       # parser и GPU-safe validator
├── geometry/            # AABB, motion bounds, pair cache, voxel fallback
├── window_context/      # GLFW_NO_API, ввод, framebuffer extent
├── gpu_pipeline/        # Vulkan runtime, ресурсы, compute и renderer facade
├── gpu_triangles/shaders/update_vertices.comp.glsl
├── gpu_intersections/shaders/intersections.comp.glsl
└── renderer/shaders/
    ├── triangle.vert.glsl
    └── triangle.frag.glsl
~~~

Подробные инварианты реализации находятся в [AGENTS.md](AGENTS.md).
