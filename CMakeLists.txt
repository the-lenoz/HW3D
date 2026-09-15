cmake_minimum_required(VERSION 3.28)

project(HW3D LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hw3d)

target_sources(
    hw3d
    PRIVATE
        src/main.cpp
        src/window_context/window_context.cpp
        src/renderer/renderer.cpp
        src/cpu_intersections/cpu_intersections.cpp
        src/gpu_intersections/gpu_intersections.cpp
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            src/window_context/window_context.cppm
            src/renderer/renderer.cppm
            src/cpu_intersections/cpu_intersections.cppm
            src/gpu_intersections/gpu_intersections.cppm
)

