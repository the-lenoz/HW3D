# HW3D: спецификация проекта

Этот файл — основная техническая спецификация репозитория. Перед изменением кода нужно прочитать его целиком и сверить описанную архитектуру с затрагиваемыми файлами.

## Назначение

HW3D читает набор треугольников в трёхмерном пространстве, находит треугольники, пересекающиеся хотя бы с одним другим треугольником, и визуализирует сцену через OpenGL. Непересекающиеся треугольники должны отображаться серыми, пересекающиеся — красными. Камера должна управляться с клавиатуры.

Предусмотрены два взаимозаменяемых способа вычисления пересечений:

- на CPU;
- на GPU с помощью GLSL compute shader.

Оба способа должны выдавать одинаковый по смыслу результат: `std::vector<bool>` длины `N`, где `false` означает, что треугольник не пересекается ни с одним другим, а `true` — что пересекается хотя бы с одним.

## Текущее состояние

Проект находится на стадии каркаса.

Реализовано:

- сборка приложения и тестов через CMake и Ninja;
- пять C++-модулей, подключённых к цели `hw3d`;
- чтение и проверка входных данных в `hw3d.configuration`;
- инициализация GLFW, создание окна и OpenGL 4.6 Core context;
- загрузка OpenGL 4.6 Core API через GLAD;
- цикл событий и кадров, завершающийся по `Esc` или закрытию окна;
- регистрация отдельных callback-функций для четырёх стрелок;
- импорт и связывание существующих модулей в `main.cpp`;
- загрузка треугольников из конфигурации в VAO/VBO;
- компиляция vertex/fragment shaders, серая окраска обычных и красная окраска отмеченных треугольников;
- чёрная экранная кайма по рёбрам каждого треугольника;
- perspective camera с захватом мыши, yaw/pitch-вращением и creative-flight перемещением;
- динамические near/far clipping planes, следующие за положением камеры относительно сцены;
- поиск пересечений треугольников на CPU через sparse voxel grid, AABB broad phase и plane/interval narrow phase;
- отдельные GoogleTest-наборы для всех пяти модулей;
- пустой GLSL compute shader как место для будущей GPU-реализации.

Пока не реализовано:

- освещение и материалы;
- загрузка, компиляция и запуск compute shader;
- поиск пересечений на GPU;
- выбор вычислительного backend;
- сборка или копирование compute shader средствами CMake.

CPU-модуль реализован, и `main` передаёт его результат renderer. GPU-модуль пока намеренно содержит только объявления `export module` и `module`.

## Структура репозитория

```text
HW3D/
├── AGENTS.md
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── tests/
│   ├── configuration_tests.cpp
│   ├── window_context_tests.cpp
│   ├── renderer_tests.cpp
│   ├── cpu_intersections_tests.cpp
│   └── gpu_intersections_tests.cpp
└── src/
    ├── main.cpp
    ├── configuration/
    │   ├── configuration.cppm
    │   └── configuration.cpp
    ├── window_context/
    │   ├── window_context.cppm
    │   └── window_context.cpp
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

Цель `hw3d_modules` связывается с `hw3d_glad`, `glfw` и `OpenGL::GL`. Renderer shaders копируются на стадии конфигурации в `${CMAKE_CURRENT_BINARY_DIR}/shaders/renderer`; абсолютный build-путь передаётся реализации через приватное определение `HW3D_RENDERER_SHADER_DIR`.

При `BUILD_TESTING=ON` — это стандартное значение CTest — CMake получает GoogleTest 1.17.0 через `FetchContent` из официального репозитория и строит пять независимых test executables. Каждый из них связывается с `hw3d_modules` и `GTest::gtest_main`. GL-тесты дополнительно связываются с `hw3d_glad`, выполняются последовательно и запускаются с отключённой переменной `WAYLAND_DISPLAY`: это выбирает стабильный для повторной инициализации GLFW X11 backend. Если во время конфигурации `DISPLAY` отсутствует, а `xvfb-run` найден, CTest автоматически оборачивает каждый GL-тест в отдельный Xvfb server. GoogleTest и его цели можно исключить из сборки через `-DBUILD_TESTING=OFF`.

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

Тесты запускаются после сборки соответствующего preset:

```bash
ctest --preset debug
ctest --preset release
```

Оба preset используют генератор Ninja и создают каталоги `build/debug` или `build/release`. Для них включён `compile_commands.json`; одноимённые test presets всегда показывают вывод упавших тестов.

При добавлении модуля с интерфейсом и реализацией нужно:

1. создать `src/<module>/<module>.cppm` и `src/<module>/<module>.cpp`;
2. добавить `.cppm` в `FILE_SET cxx_modules`;
3. добавить `.cpp` в обычные `PRIVATE` sources.

Если модуль полностью реализован в интерфейсном файле, например состоит только из шаблонов, допускается один файл `src/<module>.cppm` без отдельной подпапки и `.cpp`.

Имена C++-модулей имеют префикс `hw3d.`, а публичные сущности размещаются в пространстве имён `hw3d`.

## Общий механизм работы системы

Текущий поток управления:

```text
stdin -> Configuration
      -> cpu_collisions(Configuration::triangles)
      -> vector<bool> highlighted
      -> WindowContext создаёт окно, активный GL context и загружает GLAD
      -> Renderer принимает triangles + flags, компилирует shaders и создаёт VAO/VBO
      -> main регистрирует renderer callbacks на стрелки и мышь
      -> WindowContext::run(render_frame)
      -> WindowContext передаёт delta time движению, а mouse offsets вращению
      -> Renderer очищает buffers и рисует заполнение с каймой каждый кадр
      -> Esc/закрытие окна завершает цикл
      -> RAII сначала освобождает renderer, затем окно и GLFW
```

Целевой поток данных после реализации GPU backend:

```text
stdin
  -> hw3d.configuration
  -> Configuration::triangles
  -> hw3d.cpu_intersections или hw3d.gpu_intersections
  -> std::vector<bool> отметок размером N
  -> hw3d.renderer
  -> окно и OpenGL-контекст из hw3d.window_context
```

`main.cpp` является composition root: он должен связывать модули, выбирать вычислительный backend, передавать данные между ними и регистрировать callback-функции. Алгоритмы геометрии, детали OpenGL и работа GLFW не должны реализовываться в `main.cpp`.

Целевой жизненный цикл приложения:

1. прочитать конфигурацию из `stdin`;
2. создать окно и активный OpenGL-контекст;
3. подготовить графические ресурсы;
4. вычислить отметки пересечений выбранным backend;
5. передать треугольники и отметки renderer-модулю;
6. зарегистрировать обработчики клавиатуры и изменения размера окна;
7. выполнять цикл обработки событий, движения камеры и отрисовки;
8. освободить GPU-ресурсы до уничтожения контекста.

CPU- и GPU-модули должны соблюдать единый контракт результата. Renderer не должен знать, каким способом получены отметки. Вычислительные модули не должны зависеть от окна, камеры или логики отрисовки.

## Формат входных данных

Ввод читается из текстового `std::istream`. В `main.cpp` передаётся `std::cin`.

```text
N
x11 y11 z11
x12 y12 z12
x13 y13 z13
...
xN1 yN1 zN1
xN2 yN2 zN2
xN3 yN3 zN3
```

Ограничение: `0 < N < 1'000'000`. После `N` для каждого треугольника должны присутствовать девять координат типа `float`. Разделителями могут быть любые пробельные символы, поэтому пустые строки не имеют особого значения.

Текущий parser:

- читает `N` во временный `std::int64_t`;
- отклоняет отсутствующее, непарсящееся или выходящее за допустимый диапазон значение;
- заранее резервирует память под `N` треугольников;
- последовательно читает три вершины каждого треугольника;
- при ошибке бросает `std::runtime_error` с номером треугольника, начиная с единицы;
- не проверяет треугольники на вырожденность;
- не проверяет координаты отдельной проверкой на `NaN` или бесконечность;
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

struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

struct Configuration {
    std::vector<Triangle> triangles;
};

[[nodiscard]] Configuration read_configuration(std::istream& input);
```

`Vec3` и `Triangle` сейчас являются общими моделями геометрических данных и объявлены именно в этом модуле. Остальные модули должны импортировать и переиспользовать их, а не создавать несовместимые дубликаты. Если позднее модели будут вынесены в отдельный доменный модуль, нужно одновременно обновить все импорты и этот документ.

Ошибки ввода передаются вызывающему коду исключениями. Модуль не читает `std::cin` напрямую, не пишет диагностику и не зависит от GLFW/OpenGL.

### `hw3d.window_context`

Файлы: `src/window_context/window_context.cppm` и `src/window_context/window_context.cpp`.

Модуль реализован поверх GLFW и скрывает GLFW-типы за PImpl. Публичный контракт:

```cpp
enum class ArrowKey { up, down, left, right };

using WindowCallbackFunction = void (*)(void*) noexcept;
using MovementCallbackFunction = void (*)(void*, float) noexcept;
using MouseMoveCallbackFunction = void (*)(void*, float, float) noexcept;

struct WindowCallback {
    WindowCallbackFunction function = nullptr;
    void* context = nullptr;
};

struct MovementCallback {
    MovementCallbackFunction function = nullptr;
    void* context = nullptr;
};

struct MouseMoveCallback {
    MouseMoveCallbackFunction function = nullptr;
    void* context = nullptr;
};

class WindowContext final {
public:
    WindowContext(int width, int height, const char* title);
    ~WindowContext();

    void register_arrow_callback(ArrowKey, MovementCallback) noexcept;
    void register_mouse_move_callback(MouseMoveCallback) noexcept;
    void run(WindowCallback frame_callback = {});
    void request_close() noexcept;

    // Копирование и перемещение запрещены.
};
```

Контракт и текущее поведение:

- одновременно допускается только один живой `WindowContext`;
- создание, регистрация callback-функций, `run()` и уничтожение выполняются в одном главном потоке приложения;
- размеры должны быть положительными, а `title` — не `nullptr`;
- constructor инициализирует GLFW, запрашивает OpenGL 4.6 Core Forward-Compatible context, создаёт окно, делает context текущим, загружает GLAD и включает VSync через interval `1`;
- ошибки аргументов, повторный экземпляр, ошибка GLFW и ошибка создания окна сообщаются исключениями;
- все callback-контракты состоят из `noexcept`-функции и непрозрачного context pointer; `function == nullptr` означает отсутствие обработчика;
- при создании окна cursor переводится в `GLFW_CURSOR_DISABLED`, поэтому мышь скрыта и захвачена окном; если GLFW поддерживает raw mouse motion, он также включается;
- первое событие курсора только инициализирует предыдущую позицию, последующие передают `(x_offset, y_offset)`; положительный `y_offset` соответствует движению мыши вверх;
- удерживаемые стрелки опрашиваются каждый кадр через `glfwGetKey`, а не через системный key-repeat;
- movement callback получает `delta_seconds`; значение считается через `glfwGetTime` и ограничивается максимумом `0.1`, чтобы длинная пауза не вызвала скачок камеры;
- `Esc` выставляет GLFW window-close flag; отдельного exit-callback нет;
- `run()` обрабатывает события, вызывает callbacks удерживаемых стрелок, вызывает frame callback и меняет front/back buffers, пока window-close flag не установлен;
- `request_close()` позволяет выставить тот же флаг программно;
- framebuffer-size callback обновляет OpenGL viewport, включая первоначальный размер framebuffer;
- destructor уничтожает окно и вызывает `glfwTerminate()`.

Callback-функции обязаны быть `noexcept`, потому что GLFW вызывает свои trampolines через C API. Окно не владеет функциями или объектами по context pointers; все они должны оставаться валидными до завершения event loop.

Ответственность модуля:

- инициализация и завершение работы GLFW;
- создание и владение окном;
- создание и активация OpenGL-контекста;
- обработка событий;
- регистрация callback-функций GLFW;
- маршрутизация клавиатурного ввода в зарегистрированные callback-функции;
- захват мыши и маршрутизация относительного движения курсора;
- предоставление безопасной точки вызова отрисовки при активном контексте.

Этот модуль не должен вычислять пересечения и владеть алгоритмами рендеринга треугольников.

### `hw3d.renderer`

Файлы: `src/renderer/renderer.cppm` и `src/renderer/renderer.cpp`.

Модуль импортирует `hw3d.configuration`, скрывает OpenGL-типы за PImpl и экспортирует следующий основной API:

```cpp
class Renderer final {
public:
    Renderer(
        const std::vector<Triangle>& triangles,
        const std::vector<bool>& highlighted);
    ~Renderer();

    void render() noexcept;
    void move_forward(float delta_seconds) noexcept;
    void move_back(float delta_seconds) noexcept;
    void move_left(float delta_seconds) noexcept;
    void move_right(float delta_seconds) noexcept;
    void rotate(float x_offset, float y_offset) noexcept;

    // Копирование и перемещение запрещены.
};

void render_frame(void* renderer) noexcept;
void move_camera_forward(void* renderer, float delta_seconds) noexcept;
void move_camera_back(void* renderer, float delta_seconds) noexcept;
void move_camera_left(void* renderer, float delta_seconds) noexcept;
void move_camera_right(void* renderer, float delta_seconds) noexcept;
void rotate_camera(
    void* renderer,
    float x_offset,
    float y_offset) noexcept;
```

Свободные функции являются адаптерами для frame, movement и mouse callbacks окна: при ненулевом указателе они вызывают соответствующий метод `Renderer`, при `nullptr` ничего не делают.

Constructor не принимает `Configuration`: его контракт состоит только из массива треугольников и параллельного массива отметок. Он требует существующий активный OpenGL context и загруженный GLAD, отклоняет пустой массив треугольников и несовпадающие размеры массивов через `std::invalid_argument`. Входные массивы нужны только на время constructor: renderer преобразует и копирует их в GPU buffer, но не хранит ссылки.

При подготовке данных constructor:

- разворачивает каждый `Triangle` в три вершины по семь `float`: position `xyz`, barycentric `xyz` и highlight flag;
- повторяет значение `highlighted[i]` для всех трёх вершин треугольника `i`;
- создаёт VAO и VBO и загружает вершины с `GL_STATIC_DRAW`;
- читает скопированные vertex/fragment shader-файлы, компилирует их и линкует program;
- сообщает ошибки чтения, компиляции, линковки и отсутствующий uniform через исключения;
- включает depth test с функцией сравнения `GL_LESS`;
- вычисляет bounding box и охватывающую его сферу, ставит камеру по центру перед сценой и выбирает scale-dependent шаг движения.

Каждый `render()` очищает color/depth buffers, динамически рассчитывает near/far clipping planes, строит perspective matrix с вертикальным FOV `60°`, строит look-at view matrix из позиции и направления камеры, записывает `view_projection` uniform и вызывает `glDrawArrays(GL_TRIANGLES, ...)`. Near/far рассчитываются по расстоянию от камеры до центра охватывающей сцену сферы: радиус берётся с запасом `10%`, а near всегда положителен и не меньше `max(0.001 * radius, 0.0001)`. Поэтому вся сцена остаётся внутри depth range при приближении и удалении камеры, а при большом удалении near также растёт и не теряет без необходимости точность depth buffer. Собственная векторно-матричная математика использует `float` и column-major layout OpenGL; внешняя math-библиотека пока не нужна.

Камера начинает с yaw `-90°`, pitch `0°` и смотрит вдоль отрицательной оси Z. Mouse offsets изменяют yaw/pitch с чувствительностью `0.002` радиана на pixel; pitch ограничен диапазоном примерно `[-89°, 89°]`. Forward/back movement идёт вдоль полного направления взгляда, включая вертикальную составляющую: при взгляде вверх движение вперёд поднимает камеру как в creative flight. Left/right используют горизонтальный right vector. Скорость равна `1.5` радиуса сцены в секунду и умножается на переданный `delta_seconds`.

Vertex shader применяет `view_projection` и передаёт highlight flag и barycentric coordinates. Fragment shader выбирает серый `(0.55, 0.55, 0.55)` при `false` и красный `(0.85, 0.08, 0.08)` при `true`. Минимальная barycentric coordinate и `fwidth` формируют сглаженную чёрную кайму толщиной примерно `1.5` pixel по всем трём рёбрам. Освещение пока отсутствует.

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

Модуль реализован, импортирует только `hw3d.configuration` и экспортирует:

```cpp
[[nodiscard]] std::vector<bool> cpu_collisions(
    const std::vector<Triangle>& triangles);
```

Результат имеет ту же длину и порядок, что входной массив. Пустой массив и массив из одного треугольника возвращают только `false`. Функция не зависит от GLFW, OpenGL или renderer и может бросать исключения стандартных контейнеров при невозможности выделить память.

Broad phase выполняется следующим образом:

1. Индекс каждого входного треугольника становится его immutable unique ID.
2. Для каждого треугольника один раз вычисляется AABB.
3. Размер voxel по каждой оси равен максимальному размеру AABB любого треугольника по этой оси. Это покоординатный максимум, а не AABB одного треугольника с максимальным объёмом.
4. Если максимальный размер по оси равен нулю, для неё используется общий положительный fallback: максимум остальных размеров и `1.0F`.
5. Началом voxel-grid служит покоординатный минимум всей сцены.
6. Треугольник относится к voxel по центру его AABB. Именно AABB-center, а не centroid трёх вершин, сохраняет инвариант: если два AABB пересекаются и каждый их размер не превосходит размер voxel, координаты их voxels отличаются не более чем на единицу.
7. Координата voxel вычисляется через `floor((center - scene_min) / voxel_size)` и безопасно насыщается до диапазона `int64_t`, оставляющего место для прибавления соседних offsets.
8. Sparse grid хранится как `std::unordered_map<Voxel, std::vector<std::size_t>>` с собственным hash; пустые voxels не занимают память.

Для каждого треугольника просматриваются его voxel и 26 соседей. Пара проверяется только при `candidate_id > current_id`, поэтому одна и та же пара не вычисляется повторно. Если оба участника уже имеют `true`, проверка пары пропускается, поскольку она не может изменить результат.

Narrow phase пары:

1. Точное inclusive-пересечение AABB; непересекающиеся boxes сразу отбрасываются.
2. Масштабируемый tolerance вычисляется из `float` epsilon и максимального модуля координат пары.
3. Для невырожденных треугольников строятся нормали плоскостей и signed distances вершин до противоположной плоскости. Три вершины строго с одной стороны означают отсутствие пересечения.
4. Направление линии пересечения плоскостей — cross product их unit normals.
5. Для каждой плоскости находятся точки, в которых рёбра противоположного треугольника лежат на ней или пересекают её. Эти точки проецируются на направление линии, образуя одномерный interval.
6. Пересечение inclusive intervals означает пересечение треугольников.
7. Для параллельных coplanar-плоскостей треугольники проецируются в 2D по dominant normal axis; проверяются пересечения всех рёбер и вложение вершины.
8. Вырожденные треугольники классифицируются как point или longest-edge segment. Для них используются point/segment distance, segment/segment distance либо point/segment против полноценного треугольника.

При найденной коллизии `collisions[current_id]` и `collisions[candidate_id]` одновременно получают `true`. Касание вершиной или ребром считается пересечением.

Средняя сложность зависит от распределения по voxels и равна `O(N + K)`, где `K` — число кандидатных пар в 27 соседних ячейках. В худшем случае, когда все центры попали в одну область, остаётся `O(N²)`. Дополнительная память — `O(N + V)`, где `V` — число непустых voxels.

### `hw3d.gpu_intersections`

Файлы: `src/gpu_intersections/gpu_intersections.cppm`, `src/gpu_intersections/gpu_intersections.cpp` и `src/gpu_intersections/shaders/intersections.comp.glsl`.

Текущее состояние: C++-модуль не имеет публичного API; shader содержит только `#version 460 core` и пустой `main`. Эта версия shader предполагает OpenGL 4.6, если директива не будет изменена. CMake пока не компилирует и не копирует shader для приложения и не задаёт способ поиска файла во время выполнения. Отдельный GPU-тест получает абсолютный source-путь через приватное compile definition и проверяет компиляцию shader настоящим OpenGL driver.

Целевая ответственность:

- создание GPU buffers с геометрией и отметками;
- компиляция и запуск GLSL compute shader;
- корректная синхронизация compute-операций;
- получение `std::vector<bool>` в том же логическом формате, что у CPU backend;
- управление только теми OpenGL-ресурсами, которые относятся к вычислению пересечений.

GPU backend требует активного OpenGL-контекста. Его C++ API не должен заставлять `main.cpp` или renderer знать детали work groups, shader storage buffers и синхронизации.

### `main.cpp`

Текущее поведение:

- импортирует все пять модулей;
- вызывает `hw3d::read_configuration(std::cin)`;
- вызывает `hw3d::cpu_collisions(configuration.triangles)` и получает `highlighted`;
- создаёт окно `1280 x 720` с заголовком `HW3D`;
- создаёт `Renderer` из `configuration.triangles` и CPU-отметок после создания активного GL context;
- регистрирует `move_camera_forward`, `move_camera_back`, `move_camera_left` и `move_camera_right`, передавая адрес renderer как callback context;
- регистрирует `rotate_camera` как mouse-move callback с тем же context;
- запускает event loop с `render_frame` и тем же renderer context;
- объявляет renderer после окна, поэтому при выходе renderer уничтожается первым и освобождает OpenGL-ресурсы при ещё активном context;
- после штатного выхода из цикла возвращает `0`;
- перехватывает `std::exception`, печатает `what()` в `stderr` и возвращает `1`.

По мере реализации `main.cpp` должен оставаться тонким слоем композиции и управления жизненным циклом.

## Unit-тесты

Тесты написаны на GoogleTest и разделены по тестируемым модулям:

- `configuration_tests` содержит 10 тестов: корректное чтение одного и нескольких треугольников, whitespace, trailing data, вырожденные данные, отсутствующий и некорректный count, обе границы count и нумерацию неполного треугольника;
- `cpu_intersections_tests` содержит 23 теста публичной функции `cpu_collisions`: пустой/единичный вход, одинаковые, вложенные, раздельные, компланарные и некомпланарные треугольники, касание ребром/вершиной, порядок пары, выборочное выставление flags, sparse voxels, положительный зазор и вырожденные point/segment комбинации;
- `window_context_tests` содержит 9 тестов: валидацию constructor, запрет второго живого экземпляра, повторное создание после destruction, программное закрытие, frame callback и регистрацию пустых/заполненных callbacks во всех input slots;
- `renderer_tests` содержит 8 тестов с настоящим OpenGL context: ошибки constructor, состояние depth test, фактические gray/red pixels, все методы движения и вращения, все свободные callback adapters, `nullptr` context и нулевой viewport;
- `gpu_intersections_tests` содержит один тест, поскольку модуль пока не имеет API: он импортирует модуль, читает placeholder compute shader и проверяет его успешную компиляцию настоящим OpenGL driver.

Всего выполняется 51 GoogleTest case. Тесты обращаются только к публичному API модулей; внутренние функции implementation units покрываются транзитивно через публичные сценарии и не экспортируются специально ради тестирования. GLFW не предоставляет публичному модулю способ синтетически нажать стрелку или сдвинуть cursor, поэтому хранение callbacks проверяется регистрацией, а их маршрутизация от реальных устройств остаётся smoke/manual проверкой. При появлении injectable input backend это ограничение следует заменить автоматическими проверками значений `delta_seconds` и mouse offsets.

Три GL-набора требуют работающий OpenGL 4.6 driver. CTest запускает их последовательно, через X11, и при headless-конфигурации использует `xvfb-run`, если он доступен. Провал создания контекста считается ошибкой теста, а не молчаливым skip.

## Архитектурные границы

- Формат результата CPU и GPU должен быть единым: ровно одна отметка `bool` на входной треугольник, в исходном порядке.
- Конфигурационный модуль отвечает только за представление и чтение входа.
- Вычислительные модули отвечают только за обнаружение пересечений и формирование отметок.
- Renderer отвечает только за графические ресурсы и изображение сцены.
- Window context отвечает за окно, контекст, event loop и маршрутизацию ввода, но не знает о renderer и камере.
- `main.cpp` отвечает за связывание компонентов, но не содержит их внутренней логики.
- Связь window context и renderer инвертирована через callback-функции и `void*` context: окно вызывает зарегистрированные функции, не импортируя renderer.
- Callback-функции, переданные окну, не должны бросать исключения.
- Нельзя размещать CPU-проверку пересечений внутри renderer или window context.
- Нельзя дублировать типы `Vec3`, `Triangle` или контракт массива отметок в нескольких несовместимых вариантах.

## Проверка изменений

Минимальная проверка после изменения C++, shader или CMake:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Текущий smoke test полного запуска требует графическую сессию и поддержку OpenGL 4.6:

```bash
printf '1\n0 0 0\n1 0 0\n0 1 0\n' | ./build/debug/hw3d
```

Должно открыться окно `HW3D` с чёрной каймой у каждого треугольника. Треугольники, для которых CPU backend вернул `true`, должны быть красными, остальные — серыми. Мышь должна быть захвачена и вращать камеру. Удержание стрелок должно непрерывно перемещать камеру относительно направления взгляда, а `Esc` — штатно закрывать окно и завершать процесс с кодом `0`.

Проверить parser без открытия окна можно некорректным вводом:

```bash
printf '0\n' | ./build/debug/hw3d
```

Процесс должен завершиться с кодом `1` и диагностикой в `stderr`. Неполный треугольник должен вести себя так же.

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
