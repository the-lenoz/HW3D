cmake_minimum_required(VERSION 3.28)

project(HW3D LANGUAGES C CXX)

set(HW3D_SHADER_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_shaders.hpp")
set(HW3D_RENDERER_VERTEX_SHADER
    "${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/shaders/triangle.vert.glsl")
set(HW3D_RENDERER_FRAGMENT_SHADER
    "${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/shaders/triangle.frag.glsl")
set(HW3D_COMPUTE_SHADER
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu_intersections/shaders/intersections.comp.glsl")
set(HW3D_SHADER_GENERATOR
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake")

# IDEs index the target after configuration, before the first build.
if(NOT EXISTS "${HW3D_SHADER_HEADER}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DVERTEX_SHADER=${HW3D_RENDERER_VERTEX_SHADER}"
            "-DFRAGMENT_SHADER=${HW3D_RENDERER_FRAGMENT_SHADER}"
            "-DCOMPUTE_SHADER=${HW3D_COMPUTE_SHADER}"
            "-DOUTPUT_HEADER=${HW3D_SHADER_HEADER}"
            -P "${HW3D_SHADER_GENERATOR}"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

include(FetchContent)
include(CTest)

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

add_library(hw3d_modules STATIC)

add_custom_command(
    OUTPUT "${HW3D_SHADER_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
        "-DVERTEX_SHADER=${HW3D_RENDERER_VERTEX_SHADER}"
        "-DFRAGMENT_SHADER=${HW3D_RENDERER_FRAGMENT_SHADER}"
        "-DCOMPUTE_SHADER=${HW3D_COMPUTE_SHADER}"
        "-DOUTPUT_HEADER=${HW3D_SHADER_HEADER}"
        -P "${HW3D_SHADER_GENERATOR}"
    DEPENDS
        "${HW3D_RENDERER_VERTEX_SHADER}"
        "${HW3D_RENDERER_FRAGMENT_SHADER}"
        "${HW3D_COMPUTE_SHADER}"
        "${HW3D_SHADER_GENERATOR}"
    COMMENT "Embedding GLSL shaders"
    VERBATIM
)
add_custom_target(hw3d_shader_header
    DEPENDS "${HW3D_SHADER_HEADER}")
add_dependencies(hw3d_modules hw3d_shader_header)

target_sources(
    hw3d_modules
    PRIVATE
        src/configuration/configuration.cpp
        src/window_context/window_context.cpp
        src/shader_program/shader_program.cpp
        src/renderer/renderer.cpp
        src/cpu_intersections/cpu_intersections.cpp
        src/gpu_intersections/gpu_intersections.cpp
        "${HW3D_SHADER_HEADER}"
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            src/configuration/configuration.cppm
            src/window_context/window_context.cppm
            src/shader_program/shader_program.cppm
            src/renderer/renderer.cppm
            src/cpu_intersections/cpu_intersections.cppm
            src/gpu_intersections/gpu_intersections.cppm
)

target_include_directories(hw3d_modules
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(
    hw3d_modules
    PRIVATE
        hw3d_glad
        glfw
        OpenGL::GL
)

add_executable(hw3d src/main.cpp)
target_link_libraries(hw3d PRIVATE hw3d_modules)

if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    find_program(HW3D_XVFB_RUN_EXECUTABLE xvfb-run)

    function(add_hw3d_test target source)
        add_executable(${target} ${source})
        target_link_libraries(
            ${target}
            PRIVATE
                hw3d_modules
                GTest::gtest_main
        )
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES LABELS unit)
    endfunction()

    function(add_hw3d_gl_test target source)
        add_executable(${target} ${source})
        target_link_libraries(
            ${target}
            PRIVATE
                hw3d_modules
                hw3d_glad
                GTest::gtest_main
        )

        if(HW3D_XVFB_RUN_EXECUTABLE AND "$ENV{DISPLAY}" STREQUAL "")
            add_test(
                NAME ${target}
                COMMAND
                    "${HW3D_XVFB_RUN_EXECUTABLE}"
                    -a
                    "$<TARGET_FILE:${target}>"
            )
        else()
            add_test(NAME ${target} COMMAND ${target})
        endif()
        set_tests_properties(${target} PROPERTIES LABELS unit)
    endfunction()

    add_hw3d_test(
        configuration_tests
        tests/configuration_tests.cpp
    )
    add_hw3d_test(
        cpu_intersections_tests
        tests/cpu_intersections_tests.cpp
    )
    add_hw3d_gl_test(
        window_context_tests
        tests/window_context_tests.cpp
    )
    add_hw3d_gl_test(
        shader_program_tests
        tests/shader_program_tests.cpp
    )
    add_hw3d_gl_test(
        renderer_tests
        tests/renderer_tests.cpp
    )
    add_hw3d_gl_test(
        gpu_intersections_tests
        tests/gpu_intersections_tests.cpp
    )
    target_include_directories(
        gpu_intersections_tests
        PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
    )
    add_dependencies(gpu_intersections_tests hw3d_shader_header)

    set_tests_properties(
        window_context_tests
        shader_program_tests
        renderer_tests
        gpu_intersections_tests
        PROPERTIES
            RUN_SERIAL TRUE
            ENVIRONMENT_MODIFICATION "WAYLAND_DISPLAY=unset:"
    )
endif()
