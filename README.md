# HW3D

Визуализация пересечений треугольников в трёхмерном пространстве.

```text
src/
├── main.cpp
├── configuration/        # чтение входных данных
├── window_context/       # GLFW-контекст и ввод
├── renderer/             # OpenGL-отрисовка
├── cpu_intersections/    # поиск пересечений на CPU
└── gpu_intersections/    # поиск пересечений на GPU и GLSL
```

Сборка: CMake, Ninja, C++23 modules.
