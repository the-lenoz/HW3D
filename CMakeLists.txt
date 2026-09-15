cmake_minimum_required(VERSION 3.28)

project(HW3D LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glad)
add_subdirectory("${glad_SOURCE_DIR}/cmake" glad_cmake)
glad_add_library(
    hw3d_glad
    STATIC
    REPRODUCIBLE
    API gl:core=4.6
    EXTENSIONS NONE
)

find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hw3d)

set(HW3D_RENDERER_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders/renderer")
file(MAKE_DIRECTORY "${HW3D_RENDERER_SHADER_DIR}")
configure_file(
    src/renderer/shaders/triangle.vert.glsl
    "${HW3D_RENDERER_SHADER_DIR}/triangle.vert.glsl"
    COPYONLY
)
configure_file(
    src/renderer/shaders/triangle.frag.glsl
    "${HW3D_RENDERER_SHADER_DIR}/triangle.frag.glsl"
    COPYONLY
)

target_sources(
    hw3d
    PRIVATE
        src/main.cpp
        src/configuration/configuration.cpp
        src/window_context/window_context.cpp
        src/renderer/renderer.cpp
        src/cpu_intersections/cpu_intersections.cpp
        src/gpu_intersections/gpu_intersections.cpp
    PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            src/configuration/configuration.cppm
            src/window_context/window_context.cppm
            src/renderer/renderer.cppm
            src/cpu_intersections/cpu_intersections.cppm
            src/gpu_intersections/gpu_intersections.cppm
)

target_compile_definitions(
    hw3d
    PRIVATE HW3D_RENDERER_SHADER_DIR="${HW3D_RENDERER_SHADER_DIR}"
)

target_link_libraries(hw3d PRIVATE hw3d_glad glfw OpenGL::GL)
