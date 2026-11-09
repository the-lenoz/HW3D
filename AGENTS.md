# HW3D: спецификация проекта

Этот файл — основная техническая спецификация репозитория. Перед изменением
кода нужно прочитать его целиком и сверить инварианты с затрагиваемыми файлами.

## Назначение и поддерживаемая платформа

HW3D читает движущиеся треугольники, находит все треугольники, которые
пересекаются хотя бы с одним другим, и визуализирует сцену. Непересекающиеся
треугольники серые, пересекающиеся красные.

Поддерживается Linux + GLFW + Vulkan 1.4. Vulkan — единственный backend.
OpenGL, GLAD, VMA, CPU rotation и CPU intersections в проекте отсутствуют.
Устройство и loader обязаны поддерживать Vulkan 1.4; fallback на Vulkan 1.3
или OpenGL запрещён.

## Основной data flow

Оконный кадр:

~~~text
Configuration
  → одноразовый staging upload InitialTriangle и candidates
  → rotation compute, одновременно highlighted = 0
  → COMPUTE write → COMPUTE read/write barrier
  → float SAT над тем же runtime buffer
  → COMPUTE write → VERTEX read barrier
  → dynamic rendering читает тот же runtime buffer
  → present
~~~

Весь кадр записывается в один command buffer и отправляется одним queue
submit. Используется один кадр в полёте, один runtime buffer, один fence и две
binary semaphore. Поэтому дублирование runtime buffer не требуется.
Runtime readback отсутствует: кадры не загружают и не скачивают runtime-
геометрию.

## Сборка

Минимальная версия CMake — 3.28. Проект использует C++23 modules и Ninja.
Корневой проект объявляет только язык CXX.

Зависимости:

- GLFW 3.3+ находится через find_package;
- официальный Vulkan-Headers закреплён на v1.4.363 через FetchContent;
- системный Vulkan loader находится через find_library;
- glslc обязателен;
- spirv-val необязателен и запускается автоматически при наличии;
- GoogleTest v1.17.0 загружается только при BUILD_TESTING=ON.

Четыре GLSL shader компилируются с target-env=vulkan1.4. CMake-script
проверяет, что размер SPIR-V кратен четырём, и генерирует
generated/embedded_spirv.hpp с std::array<uint32_t, N>. Runtime не читает
shader-файлы с диска.

Основные команды:

~~~bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake --preset release
cmake --build --preset release
ctest --preset release
~~~

Debug пытается включить VK_LAYER_KHRONOS_validation и debug utils. Отсутствие
layer не является ошибкой. HW3D_DISABLE_VALIDATION отключает optional layer
для тестов, где stderr обязан быть пустым.

## Модули и структура

~~~text
src/
├── main.cpp
├── configuration/
│   ├── configuration.cppm
│   └── configuration.cpp
├── geometry/
│   ├── geometry.cppm
│   └── geometry.cpp
├── window_context/
│   ├── window_context.cppm
│   └── window_context.cpp
├── gpu_pipeline/
│   ├── gpu_pipeline.cppm
│   └── gpu_pipeline.cpp
├── gpu_triangles/shaders/update_vertices.comp.glsl
├── gpu_intersections/shaders/intersections.comp.glsl
└── renderer/shaders/
    ├── triangle.vert.glsl
    └── triangle.frag.glsl
~~~

gpu_pipeline.cpp скрывает VulkanRuntime, AllocatedBuffer, AllocatedImage,
ComputeState и RendererState за единым публичным GpuPipeline. main.cpp не
должен видеть Vulkan-типы, command buffers, descriptors или barriers.

## Конфигурация

Публичный интерфейс:

~~~cpp
struct Vec3 { float x, y, z; };

struct InitialTriangle {
    Vec3 a, b, c;
    Vec3 rotation_axis_origin{};
    Vec3 rotation_axis_direction{0.0F, 0.0F, 1.0F};
    float angular_velocity_radians_per_second = 0.0F;
};

struct Configuration {
    std::chrono::seconds simulation_duration{};
    std::vector<InitialTriangle> triangles;
};

Configuration read_configuration(std::istream&);
void validate_configuration(const Configuration&);
~~~

Validator вызывается parser-ом и повторно конструктором GpuPipeline, чтобы
проверять программно созданные Configuration.

Инварианты:

- 0 < triangles.size() < 1 000 000;
- duration положительна;
- вершины, начало/направление оси и скорость конечны;
- направление оси строго ненулевое;
- вырожденные треугольники разрешены;
- для каждой вершины double-проверка требует
  max_abs(axis_origin) + 8 × radius <= float_max / 4;
- abs(angular_velocity) × duration конечен и <= float_max / 4;
- консервативные motion bounds конечны, упорядочены и лежат в безопасном
  диапазоне.

validate_configuration бросает invalid_argument с 1-based номером triangle.
read_configuration преобразует validation failure в runtime_error, сохраняя
parser-контракт.

Формат ввода:

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

Parser нормализует направление от первой точки оси ко второй в double.
Положительная входная скорость означает clockwise при взгляде со второй точки
на первую, поэтому при переводе в радианы знак инвертируется.

## Geometry

Geometry остаётся CPU-модулем только для неизменяемой подготовки сцены:

- Aabb и motion_aabb полного оборота;
- voxel grid;
- TrianglePair;
- make_motion_pair_candidates.

CPU update_vertices, Expansion, orient2d/orient3d и adaptive exact predicates
удалены. Runtime rotation и narrow phase выполняются только shader-ами.

maximum_cached_triangle_pair_count равен 256 000 000. Если полный список
motion-AABB пар превышает переданный budget или CPU reserve получает bad_alloc,
MotionPairCandidates сохраняет voxels и pairs == nullopt.

## WindowContext

WindowContext отвечает только за GLFW window и ввод. Он выставляет
GLFW_CLIENT_API = GLFW_NO_API. Current OpenGL context, swap interval,
glfwSwapBuffers и GL viewport отсутствуют.

FrameInput содержит:

~~~cpp
bool up, down, left, right;
float mouse_x_offset, mouse_y_offset;
float delta_seconds;
uint32_t framebuffer_width, framebuffer_height;
~~~

Framebuffer callback только обновляет сохранённый extent. run собирает ввод,
ограниченный delta, mouse offsets, FPS и обновляет заголовок. Opaque native
handle предназначен только для Vulkan integration и не раскрывает GLFW/Vulkan
тип через C++ module boundary.

## GpuPipeline

Публичный контракт:

~~~cpp
struct GpuPipelineOptions {
    size_t maximum_cached_pair_count =
        maximum_cached_triangle_pair_count;
};

GpuPipeline(WindowContext&, const Configuration&, GpuPipelineOptions = {});

bool render_frame(duration<float>, const FrameInput&);
~~~

Конструктор полностью копирует/упаковывает immutable input и не сохраняет
Configuration. Elapsed обязан быть конечным и неотрицательным. WindowContext
живёт дольше pipeline.

render_frame возвращает false при framebuffer 0×0, acquire out-of-date или
невозможности представить кадр во время recreation. Suboptimal present
считается представленным кадром и помечает swapchain для recreation.

## Vulkan runtime и память

Владение выполняют Vulkan-Hpp RAII objects. Каждый logical buffer/image имеет
отдельную DeviceMemory allocation. AllocatedBuffer:

- выбирает required + preferred memory flags, затем required fallback;
- для host-visible памяти предпочитает coherent/cached;
- поддерживает non-coherent память через flush/invalidate всей allocation
  (VK_WHOLE_SIZE удовлетворяет atom alignment).

Initial, runtime, pair/static candidates и dummy buffers device-local.
Initial и активный candidate payload объединяются в один staging buffer и один
initialization submit.

Выбор physical device:

- loader и device API >= 1.4;
- dynamicRendering и synchronization2 обязательны;
- shaderFloat64 не запрашивается;
- требуется одна graphics+compute family;
- present family может отличаться, тогда swapchain concurrent;
- требуются VK_KHR_swapchain и пригодная surface;
- initial/runtime descriptor ranges должны помещаться в
  maxStorageBufferRange;
- приоритет типов: discrete, integrated, virtual, CPU.

## GPU layout

Обе записи имеют sizeof 80 и alignof 16. Host-код содержит static_assert для
размера и каждого offset.

~~~text
InitialTriangle:
  vec4 a
  vec4 b
  vec4 c
  vec4 axis_origin
  vec4 axis_direction_and_speed

RuntimeTriangle:
  uvec4 a_and_highlight
  vec4 b
  vec4 c
  vec4 min_bound
  vec4 max_bound
~~~

a_and_highlight.xyz хранит float bits вершины a, .w — atomic uint highlight.
Runtime buffer имеет только STORAGE_BUFFER usage. Runtime upload и readback
отсутствуют.

## Rotation compute

Descriptor set 0:

- binding 0 — readonly InitialTriangle buffer;
- binding 1 — writeonly RuntimeTriangle buffer.

Push constant — float elapsed_seconds. local_size_x = 64.

Shader делит направление оси на максимальный абсолютный компонент перед
нормализацией. Угол speed × elapsed сокращается по полному обороту. При
speed == 0 или reduced angle == 0 вершины копируются точно. Затем shader
пишет текущий AABB и безусловно highlighted = 0.

## Float SAT

Для каждой candidate pair:

1. inclusive AABB broad phase;
2. вычитание left.a из шести вершин;
3. деление на общий pair_scale; только при pair_scale == 0 делитель равен 1;
4. рёбра каждого triangle отдельно масштабируются по maximum component;
5. вычисляются normals;
6. degeneracy строго normal == vec3(0);
7. всегда вычисляются все 17 axes:
   2 normals, 9 edge cross products, 3 normalA×edgeA и 3 normalB×edgeB;
8. axes не нормализуются, нулевые axes не пропускаются;
9. short-circuit между axes запрещён;
10. separation строго maxA < minB || maxB < minA;
11. intersects = !degenerateA && !degenerateB && !separated.

Epsilon нигде не используется. Касание считается пересечением. При результате
true shader делает atomicOr highlight для обоих исходных ID.

Descriptor set detector:

- binding 0 — runtime;
- binding 1 — pair range или dummy;
- binding 2 — static entries или dummy;
- push constants — operation_mode и invocation_offset.

Pair cache хранится одним buffer. Данные упакованы с учётом
minStorageBufferOffsetAlignment, а descriptor ranges ограничены
maxStorageBufferRange. Между chunks barrier не нужен: они только atomicOr
один и тот же bit.

Voxel fallback хранит отсортированные 16-байтовые ivec4 entries
(x, y, z, original_id), ищет свою и 26 соседних ячеек бинарным поиском и
рассматривает пару один раз.

## Renderer

RendererState — внутренний компонент windowed GpuPipeline. Он не владеет
triangle buffer, не загружает геометрию и не отправляет queue submit.

Swapchain:

- FIFO present mode;
- предпочтение SRGB format;
- один depth image;
- dynamic rendering;
- dynamic viewport/scissor;
- recreation при resize, OUT_OF_DATE и SUBOPTIMAL;
- extent 0×0 пропускает кадр.

Graphics pipeline:

- set 0 binding 0 — readonly общий runtime buffer;
- push constant — 64-байтовая view_projection matrix;
- позиции выбираются через gl_VertexIndex, vertex buffer отсутствует;
- Vulkan depth range [0, 1], projection инвертирует Y;
- depth test/write Less;
- face culling выключен;
- two-sided Lambert lighting, ambient, серый/красный fill и barycentric
  outline сохранены.

## main и CLI

Допустима только форма без аргументов:

~~~text
hw3d
~~~

Удалённые --headless и --use-gpu-intersections, а также любой другой аргумент
дают stderr и код 1. CLI разбирается и Configuration валидируется до
GLFW/Vulkan.

main создаёт WindowContext, затем GpuPipeline. elapsed ведётся как
duration<double>, ограничивается duration и безопасно переводится в
duration<float>. Закрытие после duration запрашивается только когда конечный
кадр действительно представлен.

## Тесты и критерии

CTest labels unit/e2e. Текущая матрица:

- configuration_tests: parser, validator, NaN/Inf, axis, GPU-safe domain,
  degenerate acceptance;
- geometry_tests: motion bounds, pair cache и forced voxel fallback;
- cli_e2e: удалённые и неизвестные CLI-флаги отклоняются до GLFW/Vulkan;
- windowed_vulkan_e2e: Vulkan surface/present и завершение по duration;
- generate_triangles_e2e: generator→windowed Vulkan.

Новые изменения считаются завершёнными только после Debug и Release build,
SPIR-V validation (если tool установлен) и полного CTest на Vulkan 1.4
device. Оконные тесты должны выполняться последовательно; при отсутствии
графической сессии используется xvfb-run.
