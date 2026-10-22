# HW3D: спецификация проекта

Этот файл — основная техническая спецификация репозитория. Перед изменением кода нужно прочитать его целиком и сверить описанную архитектуру с затрагиваемыми файлами.

## Назначение

HW3D читает набор треугольников в трёхмерном пространстве, находит треугольники, пересекающиеся хотя бы с одним другим треугольником, и визуализирует сцену через OpenGL. Непересекающиеся треугольники должны отображаться серыми, пересекающиеся — красными. Камера должна управляться с клавиатуры.

Предусмотрены два взаимозаменяемых способа вычисления пересечений:

- на CPU;
- на GPU с помощью GLSL compute shader.

Оба способа записывают одинаковый по смыслу результат в поле `Triangle::highlighted`: `false` означает, что треугольник не пересекается ни с одним другим, а `true` — что пересекается хотя бы с одним.

## Текущее состояние

Проект находится на стадии каркаса.

Реализовано:

- сборка приложения и тестов через CMake и Ninja;
- семь C++-модулей, подключённых к цели `hw3d`;
- чтение и проверка входных данных в `hw3d.configuration`;
- чтение длительности симуляции, оси и угловой скорости каждого треугольника;
- разделение неизменяемых входных `InitialTriangle` и runtime-`Triangle`;
- покадровое CPU-вращение из исходных вершин по абсолютному времени без временных аллокаций;
- общие AABB- и voxel-примитивы в `hw3d.geometry` для CPU- и GPU-backend;
- swept AABB полного оборота для выбора размера voxel движущихся треугольников;
- одноразовый пакетный расчёт swept AABB и persistent CPU/GPU detector с предвычисленным списком пересекающихся motion-AABB пар либо неизменным voxel fallback;
- инициализация GLFW, создание окна и OpenGL 4.6 Core context;
- загрузка OpenGL 4.6 Core API через GLAD;
- цикл событий и кадров, завершающийся по длительности симуляции, `Esc` или закрытию окна;
- передача состояния стрелок, накопленного движения мыши и времени кадра одним обработчиком;
- импорт и связывание существующих модулей в `main.cpp`;
- покадровая запись живого массива треугольников в mapped renderer SSBO;
- встраивание двух renderer shaders и compute shader в C++-заголовок при конфигурации и сборке;
- общий модуль `hw3d.shader_program` для компиляции, линковки и владения OpenGL-программами;
- компиляция vertex/fragment shaders, серая окраска обычных и красная окраска отмеченных треугольников;
- матовое Lambert-освещение от параллельного источника сверху со слабым ambient;
- чёрная экранная кайма по рёбрам каждого треугольника;
- perspective camera с захватом мыши, yaw/pitch-вращением и creative-flight перемещением;
- динамические near/far clipping planes, следующие за положением камеры относительно сцены;
- поиск пересечений треугольников на CPU по предвычисленным motion-AABB парам, с voxel fallback для слишком плотных сцен, текущим AABB broad phase и adaptive exact-predicate narrow phase;
- отдельные GoogleTest-наборы для всех семи модулей;
- поиск пересечений невырожденных треугольников на GPU по SSBO предвычисленных motion-AABB пар, с voxel fallback для слишком плотных сцен и SAT narrow phase, а также CPU fallback для вырожденных входов;
- выбор CPU- или GPU-backend флагом `--use-gpu-intersections`;
- headless-режим с печатью отметок пересечений вместо запуска renderer;
- динамический заголовок видимого окна с FPS и средним временем кадра;
- process e2e-тесты headless- и оконного режимов для CPU- и GPU-backend, а также генератора входных данных.

Пока не реализовано:

- настраиваемые материалы и несколько источников света;
- полноценная обработка вырожденных треугольников непосредственно внутри GPU shader;
- scale-robust GPU narrow phase для экстремально малых невырожденных треугольников, чьи float-нормали или квадраты их длин underflow до нуля.

CPU-модуль реализован и используется по умолчанию. При флаге `--use-gpu-intersections` приложение сначала проверяет вход exact-предикатом `hw3d::is_degenerate`. Если найден хотя бы один вырожденный треугольник, приложение печатает в `stderr` ровно одно предупреждение `warning: GPU intersections require non-degenerate triangles; falling back to CPU` и выбирает CPU-backend. GPU shader получает только exact-невырожденные входные треугольники, но его float SAT имеет отдельное ограничение масштаба: при экстремально малых рёбрах нормаль или `dot(normal, normal)` может underflow до нуля. Для таких данных гарантированную семантику предоставляет CPU backend; безусловная эквивалентность CPU/GPU заявляется только вне этой численной области.

## Структура репозитория

```text
HW3D/
├── AGENTS.md
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── generate_triangles.py
├── cmake/
│   └── embed_shaders.cmake
├── tests/
│   ├── configuration_tests.cpp
│   ├── geometry_tests.cpp
│   ├── robust_geometry_tests.cpp
│   ├── window_context_tests.cpp
│   ├── shader_program_tests.cpp
│   ├── renderer_tests.cpp
│   ├── cpu_intersections_tests.cpp
│   ├── gpu_intersections_tests.cpp
│   ├── headless_e2e.py
│   ├── windowed_e2e.py
│   └── generate_triangles_tests.py
└── src/
    ├── main.cpp
    ├── configuration/
    │   ├── configuration.cppm
    │   └── configuration.cpp
    ├── geometry/
    │   ├── geometry.cppm
    │   ├── geometry.cpp
    │   ├── expansion.cpp
    │   └── robust_predicates.cpp
    ├── window_context/
    │   ├── window_context.cppm
    │   └── window_context.cpp
    ├── shader_program/
    │   ├── shader_program.cppm
    │   └── shader_program.cpp
    ├── renderer/
    │   ├── renderer.cppm
    │   ├── renderer.cpp
    │   └── shaders/
    │       ├── triangle.vert.glsl
    │       └── triangle.frag.glsl
    ├── cpu_intersections/
    │   ├── cpu_intersections.cppm
    │   └── cpu_intersections.cpp
    └── gpu_intersections/
        ├── gpu_intersections.cppm
        ├── gpu_intersections.cpp
        └── shaders/
            └── intersections.comp.glsl
```

Каталоги `build/`, `cmake-build-*/` и `.idea/` являются локальными артефактами и игнорируются Git.

## Сборка

Минимальная версия CMake — 3.28. Корневой проект включает языки C и C++: C нужен сгенерированной CMake-целью GLAD, собственный код использует C++23 без compiler extensions. Компилятор должен поддерживать C++ modules и сканирование их зависимостей средствами CMake. Интерфейсы модулей перечисляются в публичном `FILE_SET` типа `CXX_MODULES` статической цели `hw3d_modules`; обычные единицы трансляции являются её `PRIVATE` sources. Исполняемый файл `hw3d` содержит только `main.cpp` и связывается с `hw3d_modules`. Такое разделение позволяет приложению и тестам импортировать одну и ту же сборку модулей без дублирования реализаций.

Конфигурация требует установленные GLFW версии не ниже 3.3 и OpenGL. Они находятся через `find_package(glfw3 3.3 REQUIRED)` и `find_package(OpenGL REQUIRED)`.

GLAD 2.0.8 подключается только через CMake `FetchContent` из официального репозитория. После `FetchContent_MakeAvailable` подключается официальный CMake helper и создаётся статическая цель `hw3d_glad` для `gl:core=4.6` без extensions. Сгенерированные GLAD-файлы находятся только внутри build-каталога и не хранятся в исходном дереве. Первичная конфигурация требует Git, Python interpreter и доступ к GitHub; `REPRODUCIBLE` запрещает генератору скачивать актуальную Khronos specification поверх зафиксированной версии зависимости.

Цель `hw3d_modules` связывается с `hw3d_glad`, `glfw` и `OpenGL::GL`. Если `${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_shaders.hpp` ещё отсутствует, CMake создаёт его уже при конфигурации, чтобы IDE могла индексировать потребителей до первой сборки. При изменении любого из трёх GLSL-файлов или скрипта сборка повторно запускает `cmake/embed_shaders.cmake` и пересобирает затронутых потребителей. Заголовок содержит строки `hw3d::shaders::triangle_vert_glsl`, `hw3d::shaders::triangle_frag_glsl` и `hw3d::shaders::intersections_comp_glsl`; имена образуются заменой точек в именах файлов на подчёркивания. Заголовок остаётся внутри build-каталога; renderer и GPU-тест не ищут GLSL-файлы при запуске.

При `BUILD_TESTING=ON` — это стандартное значение CTest — CMake получает GoogleTest 1.17.0 через `FetchContent` из официального репозитория и строит восемь независимых test executables. Каждый из них связывается с `hw3d_modules` и `GTest::gtest_main`. GL-тесты дополнительно связываются с `hw3d_glad`, выполняются последовательно и запускаются с отключённой переменной `WAYLAND_DISPLAY`: это выбирает стабильный для повторной инициализации GLFW X11 backend. Если во время конфигурации `DISPLAY` отсутствует, а `xvfb-run` найден, CTest автоматически оборачивает каждый GL-тест в отдельный Xvfb server.

Кроме GoogleTest-наборов CMake находит Python interpreter и регистрирует пять process e2e-тестов. `headless_cpu_e2e` удаляет `DISPLAY` и `WAYLAND_DISPLAY`, запускает настоящий `hw3d` с `--headless` и не требует графической системы. `headless_gpu_e2e` использует тот же бинарный файл с compute backend. `windowed_cpu_e2e` и `windowed_gpu_e2e` проверяют вращение, создание renderer и автоматическое завершение по длительности. `generate_triangles_e2e` проверяет CLI генератора и передаёт его результат настоящему CPU headless binary. GL-сценарии выполняются последовательно и при отсутствии `DISPLAY` оборачиваются в Xvfb. Unit- и e2e-наборы помечены CTest labels `unit` и `e2e`. GoogleTest и все e2e-тесты можно исключить из сборки через `-DBUILD_TESTING=OFF`.

Основные команды:

```bash
cmake --preset debug
cmake --build --preset debug
```

Для оптимизированной сборки:

```bash
cmake --preset release
cmake --build --preset release
```

Для CPU-профилирования предусмотрен отдельный preset с оптимизацией `-O3`,
отладочными символами и сохранёнными frame pointers:

```bash
cmake --preset profiling
cmake --build --preset profiling
```

Он создаёт `build/profiling`, включает `compile_commands.json` и отключает
`BUILD_TESTING`, чтобы собирать только приложение и его зависимости.

Тесты запускаются после сборки соответствующего preset:

```bash
ctest --preset debug
ctest --preset release
```

Отдельные уровни можно запускать по labels:

```bash
ctest --preset debug -L unit
ctest --preset debug -L e2e
```

Все три preset используют генератор Ninja и создают каталоги `build/debug`, `build/release` или `build/profiling`. Для них включён `compile_commands.json`; test presets существуют для debug и release и всегда показывают вывод упавших тестов.

При добавлении модуля с интерфейсом и реализацией нужно:

1. создать `src/<module>/<module>.cppm` и `src/<module>/<module>.cpp`;
2. добавить `.cppm` в `FILE_SET cxx_modules`;
3. добавить `.cpp` в обычные `PRIVATE` sources.

Если модуль полностью реализован в интерфейсном файле, например состоит только из шаблонов, допускается один файл `src/<module>.cppm` без отдельной подпапки и `.cpp`.

Имена C++-модулей имеют префикс `hw3d.`, а публичные сущности размещаются в пространстве имён `hw3d`.

## Общий механизм работы системы

Текущий поток управления в оконном режиме:

```text
stdin -> Configuration
      -> update_vertices создаёт runtime-массив Triangle для t = 0
      -> calculate_motion_bounds один раз создаёт массив AABB полного движения
      -> preflight is_degenerate при запросе GPU
      -> WindowContext создаёт окно, активный GL context и загружает GLAD
      -> выбранный detector один раз создаёт список motion-AABB пар либо voxel fallback и рабочие буферы
      -> Renderer хранит ссылку на triangles, создаёт VAO/SSBO и через ShaderProgram компилирует встроенные shaders
      -> main передаёт WindowContext один C++-обработчик кадра
      -> WindowContext::run собирает FrameInput после обработки событий
      -> update_vertices пересчитывает текущие вершины по абсолютному времени
      -> detector.update обновляет Triangle::highlighted текущего кадра
      -> main применяет mouse offsets и стрелки к камере, затем вызывает Renderer::render
      -> Renderer синхронизирует SSBO из того же vector и рисует Lambert-lit заполнение с каймой
      -> simulation_duration, Esc или закрытие окна завершает цикл
      -> RAII освобождает detector и renderer до окна и GLFW
```

В `--headless`-режиме CPU backend не создаёт `WindowContext`: результат сразу печатается в `stdout` как разделённые пробелами `0` и `1`. GPU backend с полностью невырожденным входом перед вычислением создаёт невидимый context `1 x 1`, но также не создаёт renderer и не запускает event loop. GPU-запрос с вырожденным входом выполняет preflight до создания контекста и сразу использует CPU без GLFW/OpenGL.

Текущий поток данных с взаимозаменяемыми backend:

```text
stdin
  -> hw3d.configuration
  -> Configuration::simulation_duration и vector<InitialTriangle>
  -> hw3d.geometry::update_vertices(t = 0)
  -> preallocated vector<Triangle>
  -> hw3d.geometry::calculate_motion_bounds
  -> immutable vector<Aabb>
  -> preflight is_degenerate при запросе GPU
  -> persistent CpuIntersectionDetector или GpuIntersectionDetector
  -> runtime-треугольники с обновлённым полем highlighted
    -> headless: stdout
    -> оконный режим: hw3d.renderer
       -> окно и OpenGL-контекст из hw3d.window_context
```

В оконном режиме `main` накапливает ограниченные значения `FrameInput::delta_seconds`, зажимает абсолютное симуляционное время значением `simulation_duration` и перед каждым `detector.update()` вызывает `update_vertices`. Конечное состояние точно соответствует длительности, отрисовывается один раз, после чего окно получает запрос на закрытие. Headless-режим вычисляет только состояние `t = 0`. Параметры оси и скорости остаются только в `InitialTriangle`; рассчитанные один раз `motion_bounds` используются detector и renderer.

`main.cpp` является composition root: он должен связывать модули, выбирать вычислительный backend, передавать данные между ними и сопоставлять состояние ввода с методами renderer. Алгоритмы геометрии, детали OpenGL и работа GLFW не должны реализовываться в `main.cpp`.

Жизненный цикл оконного режима:

1. прочитать конфигурацию из `stdin`;
2. один раз выделить runtime-вектор и заполнить его через `update_vertices` для `t = 0`;
3. один раз выделить и заполнить массив `motion_bounds` в том же индексном порядке;
4. выполнить GPU preflight через `hw3d::is_degenerate`;
5. создать окно и активный OpenGL-контекст;
6. создать выбранный detector и renderer с невладеющими ссылками на runtime-вектор и готовые границы;
7. передать окну обработчик кадра;
8. каждый кадр обновлять абсолютное время и вершины, вызывать `detector.update()`, применять ввод и вызывать renderer;
9. после отрисовки конечного состояния запросить закрытие окна;
10. освободить detector и renderer до уничтожения контекста.

Во всех режимах CLI разбирается до чтения конфигурации, а конфигурация читается до создания окна. Флаги `--headless` и `--use-gpu-intersections` разрешены в любом порядке и не должны повторяться; неизвестный или повторный флаг завершает процесс с диагностикой и кодом `1`.

CPU- и GPU-модули должны соблюдать единый контракт результата. Renderer не должен знать, каким способом получены отметки. Вычислительные модули не должны зависеть от окна, камеры или логики отрисовки.

## Формат входных данных

Ввод читается из текстового `std::istream`. В `main.cpp` передаётся `std::cin`.

```text
N
duration_seconds
x11 y11 z11
x12 y12 z12
x13 y13 z13
axis1_1_x axis1_1_y axis1_1_z
axis1_2_x axis1_2_y axis1_2_z
angular_speed_1_degrees_per_second
...
xN1 yN1 zN1
xN2 yN2 zN2
xN3 yN3 zN3
axisN_1_x axisN_1_y axisN_1_z
axisN_2_x axisN_2_y axisN_2_z
angular_speed_N_degrees_per_second
```

Ограничение: `0 < N < 1'000'000`. `duration_seconds` является положительным целым значением, представимым типом `std::chrono::seconds::rep`. Для каждого треугольника должны присутствовать девять координат вершин, координаты двух различных конечных точек оси и конечная знаковая скорость типа `float`. Разделителями могут быть любые пробельные символы, поэтому пустые строки не имеют особого значения.

Первая точка оси сохраняется как `rotation_axis_origin`, а направление — как нормализованный вектор от первой точки ко второй. При переводе из градусов в секунду во внутренние радианы в секунду знак входной скорости инвертируется. Положительная входная скорость задаёт вращение по часовой стрелке при взгляде со второй точки оси на первую, отрицательная — против часовой стрелки; для нулевой входной скорости сохраняется положительный ноль.

`generate_triangles.py` сохраняет прежние позиционные аргументы `average_size`, `count` и `cube_size` и печатает данные в этом формате. Для каждого треугольника он проводит случайную единичную ось через centroid и независимо выбирает модуль скорости из равномерного распределения. `--duration` задаёт общую положительную длительность, а `--angular-speed` — конечную знаковую верхнюю границу модуля: положительное значение создаёт скорости в диапазоне `[0, limit]`, отрицательное — в `[limit, 0]`, ноль делает все треугольники неподвижными. Значения по умолчанию равны `10` секундам и `45` градусам в секунду. Необязательный `--seed` обеспечивает воспроизводимость геометрии, осей и скоростей. Генератор проверяет тот же диапазон количества треугольников, что и parser.

Текущий parser:

- читает `N` во временный `std::int64_t`;
- отклоняет отсутствующее, непарсящееся или выходящее за допустимый диапазон значение;
- читает длительность как отдельный целочисленный token, отклоняет отсутствующее, дробное, выходящее за диапазон или неположительное значение;
- заранее резервирует память под `N` треугольников;
- последовательно читает три вершины, две точки оси и скорость каждого треугольника;
- отклоняет совпадающие или нечисловые точки оси и нечисловую скорость;
- при ошибке данных треугольника бросает `std::runtime_error` с номером треугольника, начиная с единицы;
- не проверяет треугольники на вырожденность;
- не проверяет координаты вершин отдельной проверкой на `NaN` или бесконечность;
- не требует конца потока после последнего треугольника и игнорирует последующие данные.

## Модули

### `hw3d.configuration`

Файлы: `src/configuration/configuration.cppm` и `src/configuration/configuration.cpp`.

Модуль реализован и предоставляет следующий публичный интерфейс:

```cpp
struct Vec3 {
    float x;
    float y;
    float z;
};

struct InitialTriangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    Vec3 rotation_axis_origin{};
    Vec3 rotation_axis_direction{0.0F, 0.0F, 1.0F};
    float angular_velocity_radians_per_second = 0.0F;
};

struct Configuration {
    std::chrono::seconds simulation_duration{};
    std::vector<InitialTriangle> triangles;
};

[[nodiscard]] Configuration read_configuration(std::istream& input);
```

`Vec3` и `InitialTriangle` описывают прочитанные неизменяемые данные. У входного треугольника нет runtime-поля `highlighted`. Для программно созданного `InitialTriangle` по умолчанию используется неподвижное вращение вокруг оси `+Z`. Parser заменяет эти значения данными входа и гарантирует единичную длину `rotation_axis_direction`. Текущая геометрия и результат пересечений представлены отдельным `Triangle` из `hw3d.geometry`.

Ошибки ввода передаются вызывающему коду исключениями. Модуль не читает `std::cin` напрямую, не пишет диагностику и не зависит от GLFW/OpenGL.

### `hw3d.geometry`

Файлы: `src/geometry/geometry.cppm`, `src/geometry/geometry.cpp`, `src/geometry/expansion.cpp` и `src/geometry/robust_predicates.cpp`.

Модуль импортирует `hw3d.configuration`, владеет runtime-представлением треугольника, пересчитывает его положение и содержит общие для CPU- и GPU-backend примитивы broad phase:

```cpp
struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    bool highlighted = false;
};

void update_vertices(
    std::span<const InitialTriangle> initial,
    std::span<Triangle> current,
    std::chrono::duration<double> elapsed);

struct Aabb { Vec3 minimum; Vec3 maximum; };
struct Voxel { std::int64_t x; std::int64_t y; std::int64_t z; };
struct VoxelHash;
struct VoxelGrid { Vec3 origin; Vec3 voxel_size; };
struct TrianglePair { std::uint32_t first; std::uint32_t second; };
inline constexpr std::size_t maximum_cached_triangle_pair_count =
    256'000'000;
struct MotionPairCandidates {
    std::vector<Voxel> voxels;
    std::optional<std::vector<TrianglePair>> pairs;
};

enum class Sign : std::int8_t { negative = -1, zero = 0, positive = 1 };
template<class Scalar> struct Vector2 { Scalar x; Scalar y; };
template<class Scalar> struct Vector3 { Scalar x; Scalar y; Scalar z; };

class Expansion;
[[nodiscard]] Sign orient2d(
    const Vector2<float>& a,
    const Vector2<float>& b,
    const Vector2<float>& c);
[[nodiscard]] Sign orient3d(
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const Vec3& d);

[[nodiscard]] Aabb aabb(const Triangle& triangle) noexcept;
[[nodiscard]] Aabb motion_aabb(const InitialTriangle& triangle) noexcept;
void calculate_motion_bounds(
    std::span<const InitialTriangle> initial,
    std::span<Aabb> output);
[[nodiscard]] Vec3 extent(const Aabb& bounds) noexcept;
[[nodiscard]] Vec3 center(const Aabb& bounds) noexcept;
[[nodiscard]] bool aabbs_intersect(const Aabb&, const Aabb&) noexcept;
[[nodiscard]] VoxelGrid make_voxel_grid(
    std::span<const Aabb> bounds) noexcept;
[[nodiscard]] Voxel voxel_for(
    const Vec3& point,
    const VoxelGrid& grid) noexcept;
[[nodiscard]] MotionPairCandidates make_motion_pair_candidates(
    std::span<const Aabb> motion_bounds,
    std::size_t maximum_pair_count);
```

`update_vertices` требует одинакового размера входного и заранее выделенного выходного span, а также конечного неотрицательного абсолютного времени; нарушение контракта сообщает `std::invalid_argument`. Функция не выполняет динамических аллокаций. Для каждого движущегося треугольника она проверяет конечность начала оси, нормализует направление оси в `double`, отклоняя нулевое или нечисловое направление, сокращает угол `angular_velocity * elapsed` через `remainder` полного оборота, строит одну матрицу Родрига и применяет её к радиус-векторам трёх исходных вершин относительно `rotation_axis_origin`. Поэтому программно созданные пропорциональные направления задают одно и то же вращение, а runtime-вершины остаются внутри рассчитанного тем же образом `motion_aabb`. Нулевая скорость копирует исходные `float` без проверки неиспользуемой оси и без тригонометрии; нулевой угол после проверки оси также копирует исходные вершины. Функция изменяет только `a/b/c`; существующее значение `highlighted` сохраняется.

`aabb` возвращает статические границы текущих runtime-вершин. `motion_aabb` принимает неизменяемый `InitialTriangle`; при скорости, точно равной нулю, он возвращает box исходных вершин. Для любой ненулевой скорости функция строит консервативный AABB полного оборота: нормализует направление оси, для каждой вершины находит центр и радиус окружности вокруг оси с учётом `rotation_axis_origin`, вычисляет экстремумы окружности по мировым X/Y/Z и объединяет их. Результат округляется наружу до `float`. Некорректное программно заданное направление или начало оси безопасно возвращает статический AABB; parser таких данных не создаёт.

`calculate_motion_bounds` без динамических аллокаций заполняет заранее выделенный массив в том же индексном порядке. Функция требует равенства размеров, конечности вершин, начала/направления оси и скорости, ненулевого направления оси и гарантирует конечные упорядоченные границы. `main` вызывает её ровно один раз при подготовке сцены; detector и renderer сами `motion_aabb` не пересчитывают.

`make_voxel_grid` выбирает origin как минимум всех переданных AABB, а размер ячейки как покоординатный максимум их размеров. Нулевые размеры заменяются общим положительным fallback. `voxel_for` применяет `floor` и насыщает координаты до безопасного диапазона `int64_t`, оставляя место для соседних offsets. Пустой набор AABB создаёт сетку с origin `(0,0,0)` и размером ячейки `(1,1,1)`.

`make_motion_pair_candidates` валидирует готовые `motion_bounds`, вычисляет статический voxel каждого ID в `double` и через sparse grid рассматривает только собственную и 26 соседних ячеек. В результат включаются один раз пары `first < second`, motion AABB которых действительно пересекаются включительно. Если число пар не превышает переданный предел, `pairs` содержит полный непрерывный список; при превышении предела или невозможности расширить только pair-vector возвращается `nullopt`, а массив `voxels` сохраняется для fallback. Размер входа должен помещаться в 32-битные ID.

`orient2d` и `orient3d` возвращают математически точный знак determinant относительно уже округлённых входных `float`. Обычный путь вычисляет determinant в `double` и принимает его знак только за пределами доказанной IEEE binary64 error bound. Неопределённый случай пересчитывается через `Expansion`: неперекрывающиеся `double`-компоненты хранятся от младшей к старшей в динамическом `std::vector`, суммы используют error-free transforms, а остаток произведения получается явным `std::fma`. Пустой expansion является каноническим нулём. Для этих implementation units запрещены fast-math и неявная contraction обычных выражений.

Модуль не содержит алгоритм поиска пересечений и не зависит от OpenGL.

### `hw3d.window_context`

Файлы: `src/window_context/window_context.cppm` и `src/window_context/window_context.cpp`.

Модуль реализован поверх GLFW и скрывает GLFW-типы за PImpl. Публичный контракт:

```cpp
struct FrameInput {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    float mouse_x_offset = 0.0F;
    float mouse_y_offset = 0.0F;
    float delta_seconds = 0.0F;
};

class WindowContext final {
public:
    WindowContext(
        int width,
        int height,
        const char* title,
        bool headless = false);
    ~WindowContext();

    void run(std::function<void(const FrameInput&)> on_frame = {});
    void request_close() noexcept;

    // Копирование и перемещение запрещены.
};
```

Контракт и текущее поведение:

- одновременно допускается только один живой `WindowContext`;
- создание, `run()` и уничтожение выполняются в одном главном потоке приложения;
- размеры должны быть положительными, а `title` — не `nullptr`;
- constructor инициализирует GLFW, запрашивает OpenGL 4.6 Core Forward-Compatible context, создаёт видимое или невидимое окно, делает context текущим и загружает GLAD;
- `headless == false` является значением по умолчанию, создаёт видимое окно, включает VSync через interval `1`, регистрирует callbacks и настраивает ввод;
- `headless == true` выставляет `GLFW_VISIBLE = GLFW_FALSE`, но оставляет активный context и загруженный GLAD для GPU compute; VSync, callbacks ввода, захват cursor и raw mouse motion в этом режиме не включаются;
- ошибки аргументов, повторный экземпляр, ошибка GLFW и ошибка создания окна сообщаются исключениями;
- при создании видимого окна cursor переводится в `GLFW_CURSOR_DISABLED`, поэтому мышь скрыта и захвачена окном; если GLFW поддерживает raw mouse motion, он также включается;
- первое событие курсора только инициализирует предыдущую позицию, последующие накапливают смещения до следующего кадра; положительный `mouse_y_offset` соответствует движению мыши вверх;
- удерживаемые стрелки опрашиваются каждый кадр через `glfwGetKey`, а не через системный key-repeat;
- `delta_seconds` считается через `glfwGetTime` и ограничивается максимумом `0.1`, чтобы длинная пауза не вызвала скачок камеры;
- примерно раз в секунду заголовок видимого окна меняется на `<исходный title> | FPS: ... (... ms/frame)`; обе метрики считаются по одному фактическому интервалу;
- `Esc` выставляет GLFW window-close flag; отдельного exit-callback нет;
- `run()` обрабатывает события, прекращает цикл при window-close flag, формирует `FrameInput`, обнуляет накопленные смещения мыши, вызывает один C++-обработчик кадра, если он задан, и меняет front/back buffers;
- `request_close()` позволяет выставить тот же флаг программно;
- framebuffer-size callback обновляет OpenGL viewport, включая первоначальный размер framebuffer;
- destructor уничтожает окно и вызывает `glfwTerminate()`.

Только внутренние обработчики GLFW обязаны быть `noexcept`, поскольку вызываются через C API. Они не вызывают пользовательский код. Обработчик `on_frame` вызывается из C++-цикла; его исключение выходит из `run()` обычным способом. `FrameInput` действителен на время вызова обработчика; если он сохраняет ссылки на другие объекты, они должны оставаться живыми до завершения `run()`.

Ответственность модуля:

- инициализация и завершение работы GLFW;
- создание и владение окном;
- создание и активация OpenGL-контекста;
- обработка событий;
- регистрация callback-функций GLFW;
- опрос удерживаемых стрелок и формирование состояния ввода за кадр;
- захват мыши и накопление её относительного движения между кадрами;
- предоставление безопасной точки вызова отрисовки при активном контексте.

Этот модуль не должен вычислять пересечения и владеть алгоритмами рендеринга треугольников.

### `hw3d.shader_program`

Файлы: `src/shader_program/shader_program.cppm` и `src/shader_program/shader_program.cpp`.

Модуль скрывает OpenGL-типы в реализации и экспортирует перемещаемый, некопируемый владеющий объект:

```cpp
class ShaderProgram final {
public:
    static ShaderProgram graphics(
        std::string_view vertex_source,
        std::string_view fragment_source);
    static ShaderProgram compute(std::string_view source);
    ~ShaderProgram();
    ShaderProgram(ShaderProgram&&) noexcept;
    ShaderProgram& operator=(ShaderProgram&&) noexcept;

    void use() const noexcept;
    int uniform_location(const char* name) const noexcept;
};
```

Фабрики компилируют указанные стадии, линкуют программу и удаляют промежуточные shader objects. При ошибке освобождаются уже созданные объекты; исключение содержит название стадии либо сообщение о линковке и журнал драйвера. Деструктор удаляет программу. Для всех вызовов и уничтожения нужен активный OpenGL-контекст и загруженный GLAD. Модуль не зависит от окна, renderer или алгоритма пересечений и не управляет uniforms, рисованием, compute dispatch, буферами или барьерами.

### `hw3d.renderer`

Файлы: `src/renderer/renderer.cppm` и `src/renderer/renderer.cpp`.

Модуль импортирует `hw3d.geometry` и `hw3d.shader_program`, скрывает OpenGL-типы за PImpl и экспортирует следующий основной API:

```cpp
class Renderer final {
public:
    Renderer(
        std::span<const Triangle> triangles,
        std::span<const Aabb> motion_bounds);
    ~Renderer();

    void render();
    void move_forward(float delta_seconds) noexcept;
    void move_back(float delta_seconds) noexcept;
    void move_left(float delta_seconds) noexcept;
    void move_right(float delta_seconds) noexcept;
    void rotate(float x_offset, float y_offset) noexcept;

    // Копирование и перемещение запрещены.
};
```

Renderer не импортирует `hw3d.window_context`: сопоставление `FrameInput` с методами камеры выполняет `main.cpp`.

Constructor не принимает `Configuration`: его контракт состоит из span текущих треугольников и готовых `motion_bounds` того же размера. Он требует существующий активный OpenGL context и загруженный GLAD, отклоняет пустой или несогласованный массив и некорректные границы через `std::invalid_argument` и сохраняет невладеющий `std::span<const Triangle>`. Исходное хранилище, его адрес и размер обязаны оставаться неизменными весь срок жизни renderer; менять сами элементы разрешено. `resize`, `reserve`, `swap` и любая другая операция, инвалидирующая span, запрещены и renderer не может обнаружить такое нарушение. Границы нужны только constructor для настройки камеры и не сохраняются.

При подготовке данных constructor:

- создаёт пустой VAO, необходимый OpenGL Core для `glDrawArrays` без vertex attributes;
- создаёт SSBO размера `64 * triangle_count` байт с usage `GL_STREAM_DRAW`;
- создаёт `ShaderProgram` из встроенных в заголовок vertex/fragment shaders;
- сообщает ошибки компиляции, линковки и отсутствующий uniform через исключения;
- включает depth test с функцией сравнения `GL_LESS`;
- объединяет переданные `motion_bounds`, вычисляет охватывающую сферу полного движения, ставит камеру по центру перед сценой и выбирает scale-dependent шаг движения; `motion_aabb` renderer не вызывает.

Каждый `render()` сначала проверяет viewport и при нулевой ширине или высоте возвращается без map, вычисления нормалей и очистки framebuffer. Иначе он отображает весь SSBO через `glMapNamedBufferRange` с `GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT`, записывает актуальные данные из исходного span непосредственно в mapped storage и выполняет unmap. Одна GPU-запись содержит четыре `vec4`: позиции `a/b/c`, highlight в `a.w` и одну плоскую unit normal; для вырожденной или нечисловой нормали используется `(0, 1, 0)`. Ошибка map/unmap сообщается `std::runtime_error`. Промежуточного `std::vector<float>` и трёхкратного дублирования normal/highlight нет.

После синхронизации `render()` очищает color/depth buffers, динамически рассчитывает near/far clipping planes, строит perspective matrix с вертикальным FOV `60°`, строит look-at view matrix из позиции и направления камеры, записывает `view_projection` uniform, заново привязывает renderer SSBO к binding `0` и вызывает `glDrawArrays(GL_TRIANGLES, ...)`. Повторное связывание обязательно, поскольку GPU collision backend использует тот же binding для собственного SSBO. Near/far рассчитываются по расстоянию от камеры до центра охватывающей сцену сферы: радиус берётся с запасом `10%`, а near всегда положителен и не меньше `max(0.001 * radius, 0.0001)`. Поэтому полная траектория сцены остаётся внутри depth range, а при большом удалении near также растёт и не теряет без необходимости точность depth buffer. Собственная векторно-матричная математика использует `float` и column-major layout OpenGL; внешняя math-библиотека пока не нужна.

Камера начинает с yaw `-90°`, pitch `0°` и смотрит вдоль отрицательной оси Z. Mouse offsets изменяют yaw/pitch с чувствительностью `0.002` радиана на pixel; pitch ограничен диапазоном примерно `[-89°, 89°]`. Forward/back movement идёт вдоль полного направления взгляда, включая вертикальную составляющую: при взгляде вверх движение вперёд поднимает камеру как в creative flight. Left/right используют горизонтальный right vector. Скорость равна `1.5` радиуса сцены в секунду и умножается на переданный `delta_seconds`.

Vertex shader получает индекс треугольника и локальный индекс вершины из `gl_VertexID`, читает одну запись SSBO, выбирает `a/b/c` и формирует barycentric coordinates без хранения их в памяти. Затем он применяет `view_projection` и передаёт highlight flag и плоскую normal. Fragment shader выбирает базовый цвет: серый `(0.55, 0.55, 0.55)` при `false` или красный `(0.85, 0.08, 0.08)` при `true`. Затем он применяет двухстороннее по отношению к видимой грани матовое Lambert-освещение. Направление от поверхности к параллельному источнику задаётся world-space вектором `normalize((-0.35, 1.0, 0.45))`: положительная Y-компонента размещает источник сверху, а X/Z-компоненты не оставляют все вертикальные поверхности только на ambient. Интенсивность равна `0.15 + 0.85 * max(dot(normal, direction_to_light), 0)`. Specular-слагаемого нет, поэтому материал не создаёт глянцевых бликов. Минимальная barycentric coordinate и `fwidth` формируют поверх результата сглаженную нелитую чёрную кайму толщиной примерно `1.5` pixel по всем трём рёбрам.

Целевая ответственность:

- создание и удаление OpenGL-ресурсов визуализации;
- загрузка геометрии треугольников в GPU buffers;
- хранение или обновление буфера отметок пересечений;
- отрисовка обычных треугольников серым цветом;
- отрисовка отмеченных треугольников красным цветом;
- применение матриц модели, вида и проекции;
- настройка необходимого освещения.

Renderer принимает готовые отметки и не определяет пересечения самостоятельно. Все OpenGL-ресурсы должны создаваться и уничтожаться при существующем активном контексте.

### `hw3d.cpu_intersections`

Файлы: `src/cpu_intersections/cpu_intersections.cppm` и `src/cpu_intersections/cpu_intersections.cpp`.

Модуль реализован, импортирует `hw3d.geometry`, скрывает состояние за PImpl и экспортирует:

```cpp
class CpuIntersectionDetector final {
public:
    CpuIntersectionDetector(
        std::vector<Triangle>& triangles,
        std::span<const Aabb> motion_bounds,
        std::size_t maximum_pair_count =
            maximum_cached_triangle_pair_count);
    ~CpuIntersectionDetector();
    void update();

    // Копирование и перемещение запрещены.
};
```

Constructor сохраняет невладеющую ссылку на объект `std::vector`, проверяет совпадение его размера с `motion_bounds`, корректность границ и pair-бюджет не выше `maximum_cached_triangle_pair_count`, а затем один раз строит статический broad phase и рабочие массивы. Вектор обязан жить дольше detector и не менять размер. Необязательный бюджет позволяет заранее выбрать voxel fallback; значение по умолчанию остаётся равным 256 миллионам пар. `update()` проверяет инвариант размера и конечность всех координат, очищает `highlighted`, но в pair-режиме обновляет текущие AABB и exact-примитивы только для ID, входящих хотя бы в одну предвычисленную пару. Пустой pair-cache поэтому не выполняет narrow-phase подготовку. Модуль не зависит от GLFW, OpenGL или renderer.

Broad phase выполняется следующим образом:

1. Индекс каждого входного треугольника становится его immutable unique ID.
2. Constructor получает уже вычисленный AABB полного оборота для каждого треугольника; повторно `motion_aabb` не вызывает.
3. Размер voxel по каждой оси равен максимальному размеру rotation-aware AABB любого треугольника по этой оси. Это покоординатный максимум, а не AABB одного треугольника с максимальным объёмом. Общий builder переводит границы в `double` до вычисления размеров. Текущий AABB по-прежнему используется для покадровой проверки.
4. Если максимальный размер по оси равен нулю, для неё используется общий положительный fallback: максимум остальных размеров и `1.0`.
5. Началом voxel-grid служит покоординатный минимум всей сцены.
6. Треугольник навсегда относится к voxel по центру его motion AABB. Именно AABB-center, а не centroid трёх вершин, сохраняет инвариант: если два текущих AABB пересеклись, содержащие их motion AABB также пересекаются, а координаты их voxels отличаются не более чем на единицу.
7. Координата voxel вычисляется в `double` через `floor((center - scene_min) / voxel_size)` и безопасно насыщается до диапазона `int64_t`, оставляющего место для прибавления соседних offsets. Входные AABB остаются точными boxes хранящихся `float`-координат, но вычитание больших близких мировых значений не выполняется во `float`.
8. Sparse grid при инициализации находит пары соседних voxels и оставляет только пары с inclusive-пересечением самих `motion_bounds`.

Полный список уникальных пар хранится как непрерывный `std::vector<TrianglePair>` с 32-битными ID. Общий верхний предел равен `maximum_cached_triangle_pair_count`, то есть 256 миллионам пар (2 048 000 000 байт, около 1,91 GiB); constructor может получить меньший бюджет. Если список превысил выбранный предел или его storage невозможно расширить, constructor сохраняет sparse grid и использует прежний покадровый обход собственного и 26 соседних voxels. В быстром режиме voxel map и fallback-координаты после инициализации не хранятся; отдельно остаются только текущие AABB и маска активных ID. В обоих режимах пара пропускается, если оба участника уже имеют `true`, поскольку она не может изменить результат.

Narrow phase пары:

1. Точное inclusive-пересечение AABB; непересекающиеся boxes сразу отбрасываются.
2. Каждый активный текущий треугольник один раз за `update()` материализуется в заранее выделенном `std::variant<Point, Segment, TriangleSurface>`. Точка требует равенства всех трёх вершин. Segment определяется нулевыми `orient2d` во всех трёх координатных проекциях; его концами становятся крайние вершины по первой изменяющейся координате. `TriangleSurface` хранит вершины и первую невырожденную проекцию. Текущие AABB хранятся отдельным массивом, а fallback voxel — только при фактическом fallback, поэтому narrow phase не знает об индексировании sparse grid.
3. Все топологические решения принимаются только по `orient2d` и `orient3d`; геометрического tolerance, локальной нормализации, distance-запросов и approximate SAT в CPU narrow phase нет.
4. Point/point использует точное равенство, point/segment — коллинеарность во всех проекциях и inclusive bounds, point/triangle — coplanarity и inclusive point-in-triangle в невырожденной проекции.
5. Segment/segment сначала проверяет coplanarity через `orient3d`, затем выбирает проекцию, не схлопывающую взаимное расположение. Коллинеарный случай сводится к inclusive overlap трёхмерных boxes.
6. Segment/triangle сравнивает точные знаки концов относительно плоскости. Coplanar случай сводится к двумерным edge/containment-проверкам; пересечение плоскости проверяется ориентациями вокруг отрезка без построения округлённой точки пересечения.
7. Triangle/triangle проверяет рёбра обоих треугольников против второго triangle. Это покрывает некомпланарное пересечение, coplanar containment, общую вершину и общее ребро.

Перед каждым broad phase текущая координата проверяется через `std::isfinite`; `NaN` или infinity приводят к `std::invalid_argument` с сообщением `triangle coordinates must be finite`. `update()` не перестраивает список пар или fallback grid и не создаёт динамических контейнеров.

При найденной коллизии `triangles[current_id].highlighted` и `triangles[candidate_id].highlighted` одновременно получают `true`. Касание вершиной или ребром считается пересечением.

В быстром режиме сложность кадра равна `O(N + P)`, а дополнительная память — `O(N + P)`, где `P` — число пар пересекающихся motion AABB. В fallback средняя сложность остаётся `O(N + K)` и память `O(N + V)`, где `K` — число пар в соседних ячейках, а `V` — число непустых voxels. В плотном худшем случае оба режима выполняют `O(N²)` проверок.

### `hw3d.gpu_intersections`

Файлы: `src/gpu_intersections/gpu_intersections.cppm`, `src/gpu_intersections/gpu_intersections.cpp` и `src/gpu_intersections/shaders/intersections.comp.glsl`.

Модуль скрывает состояние за PImpl и экспортирует persistent-объект:

```cpp
class GpuIntersectionDetector final {
public:
    GpuIntersectionDetector(
        std::vector<Triangle>& triangles,
        std::span<const Aabb> motion_bounds,
        std::size_t maximum_pair_count =
            maximum_cached_triangle_pair_count);
    ~GpuIntersectionDetector();
    void update();

    // Копирование и перемещение запрещены.
};
```

Constructor, `update()` и destructor требуют активный OpenGL context. Constructor сохраняет невладеющую ссылку на объект `std::vector`, проверяет совпадение размеров, корректность motion bounds и pair-бюджет не выше общего максимума, один раз компилирует compute program, создаёт buffers и заранее выделяет CPU staging/result arrays. Необязательный бюджет по умолчанию равен 256 миллионам пар и может быть уменьшен для раннего выбора fallback. Вектор обязан жить дольше detector и не менять размер.

Подготовка и выполнение устроены следующим образом:

1. Constructor запрашивает общий список максимум из выбранного числа motion-AABB пар, не превышающего `256'000'000`. Значение `GL_MAX_SHADER_STORAGE_BLOCK_SIZE` ограничивает размер одного pair SSBO, но не общий кэш.
2. Каждая GPU-запись текущей геометрии занимает 80 байт: три padded `vec3` и padded minimum/maximum AABB. Записи хранятся в исходном ID-порядке.
3. В быстром режиме binding `2` последовательно получает один или несколько неизменяемых SSBO из `uvec2` пар. Каждый SSBO не превышает лимит драйвера, один invocation обрабатывает одну пару, а буферы больше одного допустимого dispatch дополнительно разбиваются на порции через uniform `pair_offset`.
4. Если полный список не помещается либо хотя бы один pair SSBO получает `GL_OUT_OF_MEMORY`, constructor освобождает уже созданные pair buffers, сортирует отдельный binding `3` из `ivec4(static voxel, original ID)` и включает voxel fallback. В нём invocation обрабатывает одну отсортированную запись, перебирает 27 ячеек и находит bucket 22-шаговым binary search с `GL_ARB_shader_group_vote`.
5. Binding `0` содержит текущую геометрию, а binding `1` — выходные flags. `update()` обновляет geometry buffer через `glNamedBufferSubData` и очищает result buffer без пересоздания объектов.
6. В обоих режимах сначала выполняется inclusive AABB-проверка, затем SAT по 17 осям: две нормали треугольников, девять cross products рёбер и шесть лежащих в плоскостях нормалей к рёбрам. Inclusive overlap проекций считает касание пересечением.
7. При пересечении `atomicOr` выставляет flags обоих исходных индексов. После dispatch C++ выполняет memory barrier и одним `glGetNamedBufferSubData` читает результат в заранее выделенный массив, затем переносит flags в runtime-треугольники.

Каждый `update()` одним CPU-проходом проверяет неизменность размера и конечность текущих координат, очищает `highlighted` и при наличии работы заполняет geometry staging. Пустой вход и быстрый режим с пустым списком пар обрабатываются без заполнения staging, загрузки SSBO и dispatch. Между кадрами сохраняются программа, GL buffers, pair cache либо статический sorted voxel-order и CPU geometry/result arrays; временный CPU-массив отсортированных fallback entries освобождается сразу после загрузки в SSBO. Сортировка и динамические аллокации не повторяются.

До запуска GPU shader `main.cpp` проверяет каждый вход exact-предикатом `hw3d::is_degenerate`. Если найден point или segment, задаваемый вырожденным треугольником, GPU не запускается: печатается одно предупреждение в `stderr`, а результат вычисляет CPU-backend. Для прошедших preflight невырожденных треугольников SAT использует проверку `dot(normal, normal) > 0.0`, без абсолютного порога площади. Однако вычисление остаётся во `float`: у экстремально малых треугольников сама нормаль либо квадрат её длины может underflow до нуля, и GPU способен вернуть результат, отличный от CPU. Для такой геометрии следует выбирать CPU backend. CMake встраивает shader в `embedded_shaders.hpp`; GPU-тесты вызывают весь публичный pipeline на настоящем OpenGL driver.

Целевая ответственность:

- создание GPU buffers с геометрией и отметками;
- создание программы через `hw3d.shader_program` и запуск GLSL compute shader;
- корректная синхронизация compute-операций;
- запись `Triangle::highlighted` в том же логическом формате, что у CPU backend;
- управление только теми OpenGL-ресурсами, которые относятся к вычислению пересечений.

GPU backend требует активного OpenGL-контекста только при фактическом GPU-запуске. Его C++ API не должен заставлять `main.cpp` или renderer знать детали work groups, shader storage buffers и синхронизации.

### `main.cpp`

Текущее поведение:

- импортирует конфигурацию, окно, renderer и оба модуля пересечений;
- принимает флаги `--headless` и `--use-gpu-intersections` в любом порядке, отклоняет неизвестные и повторные опции;
- после разбора CLI вызывает `hw3d::read_configuration(std::cin)` до создания окна;
- один раз выделяет runtime-вектор и массив motion bounds, заполняет их через `update_vertices(t = 0)` и `calculate_motion_bounds` и сохраняет размеры на всё время работы;
- при GPU-флаге выполняет `hw3d::is_degenerate` для runtime-треугольников в начальном положении;
- при найденной вырожденной геометрии один раз печатает `warning: GPU intersections require non-degenerate triangles; falling back to CPU` в `stderr` и выбирает CPU-backend;
- в CPU headless-режиме создаёт `CpuIntersectionDetector`, вызывает `update()` без GLFW, печатает отметки через пробел и завершает процесс;
- в GPU headless-режиме с невырожденным входом создаёт невидимый context `1 x 1` и `GpuIntersectionDetector`, вызывает `update()`, печатает отметки и завершает процесс без renderer/event loop;
- в оконном режиме создаёт видимое окно `1280 x 720`, один detector выбранного backend и `Renderer`; detector получает runtime-вектор и motion bounds, renderer использует те же данные;
- передаёт `WindowContext::run` одну лямбду, захватывающую состояние приложения по ссылке;
- внутри лямбды накапливает ограниченный `delta_seconds`, зажимает время длительностью конфигурации, вызывает `update_vertices`, `detector.update()`, применяет управление камерой и вызывает `Renderer::render()`;
- после отрисовки точного конечного состояния вызывает `WindowContext::request_close()`;
- объявляет detector и renderer после окна, поэтому при выходе их OpenGL-ресурсы освобождаются при ещё активном context;
- после штатного выхода из цикла возвращает `0`;
- перехватывает `std::exception`, печатает `what()` в `stderr` и возвращает `1`.

По мере реализации `main.cpp` должен оставаться тонким слоем композиции и управления жизненным циклом.

## Тесты

Тесты написаны на GoogleTest и разделены по тестируемым модулям:

- `configuration_tests` содержит 18 тестов: корректное чтение длительности и `InitialTriangle`, нормализацию оси, перевод обычных и предельных конечных скоростей в знаковые радианы без overflow, положительный ноль, whitespace, trailing data, вырожденные данные, ошибки count и duration, нумерацию неполного треугольника, неполную или вырожденную ось и отсутствующую скорость;
- `geometry_tests` содержит 28 тестов: абсолютное clockwise-вращение, нулевое время и скорость, смещённую, произвольную и масштабированную ось, containment runtime-вершин внутри motion bounds, отсутствие накопительной ошибки, сохранение `highlighted`, ошибки аргументов и оси, `motion_aabb` исходного треугольника, пакетный расчёт motion bounds, влияние полного оборота на voxel size, inclusive-пересечение AABB, voxel-сетку, пустой/точный/превышенный бюджет motion-pair cache, face/edge/vertex-касания, zero-extent boxes, уникальность плотного набора пар и exact-проверку вырожденности;
- `robust_geometry_tests` содержит 10 тестов expansion-арифметики и predicates: сохранение младшей компоненты, точное сокращение и произведение с FMA-residual, знаки и перестановки, adversarial determinants внутри error bound, integer oracle и инвариантность при представимых переносе и двоичном масштабе;
- `cpu_intersections_tests` содержит 47 тестов `CpuIntersectionDetector`: пустой/единичный вход, сброс прежних отметок, повторные `update()`, статические и плотные motion-пары, принудительный voxel fallback, настраиваемый бюджет, интеграцию вращения с масштабированной осью, reallocation runtime-вектора без изменения размера, запрет изменения размера, одинаковые, вложенные, раздельные, компланарные, некомпланарные и почти параллельные треугольники, касание ребром/вершиной, sparse voxels, вырожденные point/segment комбинации, большие переносы, малые масштабы, metamorphic-набор и отклонение нечисловых координат даже при пустом pair-cache;
- `window_context_tests` содержит 11 тестов: валидацию constructor, запрет второго живого экземпляра, повторное создание после destruction, невидимость headless-контекста и последующее создание видимого окна, закрытие до кадра, снимок ввода за один кадр, FPS/ms title на управляемом времени, пустой обработчик и передачу исключения из обработчика;
- `shader_program_tests` содержит 5 GL-тестов: graphics/compute программы, ошибки компиляции и линковки, использование программы и перемещение владельца;
- `renderer_tests` содержит 10 тестов с настоящим OpenGL context: ошибки constructor и согласованности motion bounds, состояние depth test, live-обновления highlight и вершин исходного span, фактические gray/red pixels, сравнение слабого ambient с направленным diffuse, методы движения и вращения и нулевой viewport;
- `gpu_intersections_tests` содержит 28 GL-тестов `GpuIntersectionDetector`, включая линковку shader, повторные `update()`, статические и плотные motion-пары, принудительный voxel fallback, настраиваемый бюджет, интеграцию вращения с масштабированной осью, reallocation runtime-вектора, запрет изменения размера, сброс прежних отметок, обычные и малые невырожденные треугольники, геометрические случаи, исходный ID-порядок, границы 64/65 invocation и pair dispatch через несколько work groups.

Всего выполняется 157 GoogleTest case. Тесты обращаются только к публичному API модулей; внутренние функции implementation units покрываются транзитивно через публичные сценарии и не экспортируются специально ради тестирования. GLFW не предоставляет публичному модулю способ синтетически нажать стрелку или сдвинуть cursor, поэтому снимок ввода проверяется без искусственных событий, а реакция на реальные устройства остаётся smoke/manual проверкой. Расчёт FPS проверяется управляемым `glfwSetTime`, а чтение заголовка компилируется при GLFW 3.4 или новее. При появлении injectable input backend ограничение ввода следует заменить автоматическими проверками состояний стрелок и накопленных mouse offsets.

`headless_cpu_e2e` запускает 39 сценариев отдельными процессами без графической среды: общие невырожденные случаи, CPU-обработку point/segment-примитивов, формат ввода, ошибки count и CLI. `headless_gpu_e2e` запускает 29 сценариев: общие случаи, умеренно малый масштаб, CPU fallback для одного и нескольких вырожденных треугольников, размеры около границ workgroup и оба порядка CLI-флагов. Каждый сценарий проверяет exit code, полный `stdout`, полный `stderr` и имеет собственный timeout. Два оконных e2e запускают настоящий CPU/GPU pipeline на одну секунду и проверяют автоматическое завершение; отдельный generator e2e проверяет seed, формат, диапазоны знаковых скоростей, ошибки CLI и совместимость вывода с parser настоящего приложения.

Четыре unit GL-набора, GPU headless e2e и оба оконных e2e требуют работающий OpenGL 4.6 driver. CTest запускает их последовательно, через X11, и при headless-конфигурации использует `xvfb-run`, если он доступен. Провал создания контекста считается ошибкой теста, а не молчаливым skip.

## Архитектурные границы

- Формат результата CPU и GPU един: одно поле `Triangle::highlighted` на входной треугольник, в исходном порядке. Полная численная эквивалентность GPU не гарантируется только для документированного экстремально малого масштаба float SAT.
- `Configuration::triangles` после чтения считается неизменяемым; ось и скорость существуют только в `InitialTriangle`.
- Массивы `InitialTriangle`, runtime-`Triangle` и `motion_bounds` имеют одинаковый размер и порядок; индекс является стабильным ID.
- Runtime-вектор можно изменять поэлементно, но нельзя менять его размер, пока жив detector; detector хранит невладеющую ссылку на объект vector и проверяет сохранённый размер. Renderer хранит span: пока он жив, дополнительно запрещена любая reallocation или замена storage, а нарушение не может быть обнаружено автоматически.
- `motion_bounds[i]` неизменяем и охватывает любое положение треугольника с ID `i` на полном обороте; он вычисляется один раз до создания detector и renderer.
- `update_vertices` изменяет только вершины и сохраняет `highlighted`; detector изменяет только `highlighted`.
- Покадровый `detector.update()` не должен перестраивать motion-pair cache или voxel fallback, пересоздавать GPU resources или выделять динамические контейнеры.
- Конфигурационный модуль отвечает только за представление и чтение входа.
- Вычислительные модули отвечают только за обнаружение пересечений и формирование отметок.
- Renderer отвечает только за графические ресурсы и изображение сцены.
- `ShaderProgram` отвечает за компиляцию, линковку, выбор и освобождение OpenGL-программ; renderer и GPU backend отвечают за данные и команды конкретной задачи.
- Window context отвечает за окно, контекст, event loop и маршрутизацию ввода, но не знает о renderer и камере.
- `main.cpp` отвечает за связывание компонентов, но не содержит их внутренней логики.
- Window context передаёт `FrameInput` в один C++-обработчик кадра и не импортирует renderer; `main.cpp` сопоставляет данные ввода с методами камеры.
- Внутренние callback-функции GLFW не вызывают пользовательский код и не должны бросать исключения; исключения обработчика кадра могут выходить из `run()`.
- Нельзя размещать CPU-проверку пересечений внутри renderer или window context.
- Нельзя дублировать типы `Vec3`, `Triangle` или контракт поля `highlighted` в нескольких несовместимых вариантах.

## Проверка изменений

Минимальная проверка после изменения C++, shader или CMake:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Для раздельной проверки уровней используются:

```bash
ctest --preset debug -L unit
ctest --preset debug -L e2e
```

Текущий smoke test полного запуска требует графическую сессию и поддержку OpenGL 4.6:

```bash
printf '1\n10\n0 0 0\n1 0 0\n0 1 0\n0 0 0\n0 0 1\n90\n' | ./build/debug/hw3d
```

Должно открыться окно `HW3D` с чёрной каймой у каждого треугольника. Треугольники с `highlighted == true` должны быть красными, остальные — серыми. Мышь должна быть захвачена и вращать камеру. Удержание стрелок должно непрерывно перемещать камеру относительно направления взгляда, а `Esc` — штатно закрывать окно и завершать процесс с кодом `0`.

Проверить parser без открытия окна можно некорректным вводом:

```bash
printf '0\n' | ./build/debug/hw3d
```

Процесс должен завершиться с кодом `1` и диагностикой в `stderr`. Неполный треугольник должен вести себя так же.

Проверить CPU headless-режим без окна и графической сессии можно так:

```bash
printf '2\n10\n0 0 0\n1 0 0\n0 1 0\n0 0 0\n0 0 1\n90\n0 0 0\n1 0 0\n0 1 0\n0 0 0\n0 1 0\n45\n' \
    | ./build/debug/hw3d --headless
```

Процесс должен напечатать `1 1` и завершиться с кодом `0`. Добавление `--use-gpu-intersections` выбирает compute backend и требует доступный OpenGL 4.6 context; окно при этом остаётся невидимым.

При добавлении публичной функции или новой ветви поведения нужно расширить test-файл соответствующего модуля. Если новый тест требует graphics context, его target следует создавать через `add_hw3d_gl_test`, чтобы сохранить X11/Xvfb и serial execution contracts.

## Обязательная актуализация этого файла

Любое изменение проекта должно сопровождаться актуализацией `AGENTS.md`: разработчик обязан сверить документ с новым состоянием и исправить все затронутые разделы в том же наборе изменений. Работа не считается завершённой, пока описание снова не соответствует коду.

Обновление обязательно, если изменились:

- дерево каталогов или список файлов;
- CMake, preset, toolchain, зависимости или команды сборки;
- имя модуля, его публичный API, ответственность или зависимости;
- формат входа, валидация, сообщения и способ обработки ошибок;
- типы данных или контракт результата вычислений;
- общий поток управления и порядок жизненного цикла;
- состояние реализации: заглушка стала рабочим кодом или появился новый незавершённый компонент;
- shader, способ его сборки, доставки или загрузки;
- команды и требования тестирования.

Правила ведения документа:

1. Описывать фактическое текущее состояние отдельно от целевого поведения.
2. Не отмечать запланированную возможность как реализованную.
3. При добавлении модуля обновить дерево, общий поток данных, описание модуля и `CMakeLists.txt`.
4. При изменении публичного контракта обновить его описание и всех потребителей.
5. После правок перечитать затронутые разделы и убедиться, что по этому файлу можно собрать проект и понять границы ответственности без изучения истории Git.
