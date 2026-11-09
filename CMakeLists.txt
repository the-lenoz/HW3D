cmake_minimum_required(VERSION 3.28)

project(HW3D LANGUAGES CXX)

include(FetchContent)
include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.4.363
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(VulkanHeaders)

find_library(HW3D_VULKAN_LOADER NAMES vulkan REQUIRED)
add_library(hw3d_vulkan_loader SHARED IMPORTED GLOBAL)
set_target_properties(
    hw3d_vulkan_loader
    PROPERTIES IMPORTED_LOCATION "${HW3D_VULKAN_LOADER}")

find_package(glfw3 3.3 REQUIRED)
find_program(HW3D_GLSLC glslc REQUIRED)
find_program(HW3D_SPIRV_VAL spirv-val)

set(HW3D_SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu_triangles/shaders/update_vertices.comp.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu_intersections/shaders/intersections.comp.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/shaders/triangle.vert.glsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/shaders/triangle.frag.glsl"
)
set(HW3D_SHADER_SYMBOLS
    update_vertices_comp_spv
    intersections_comp_spv
    triangle_vert_spv
    triangle_frag_spv
)
set(HW3D_SHADER_STAGES
    compute
    compute
    vertex
    fragment
)
set(HW3D_SPIRV_FILES)
list(LENGTH HW3D_SHADER_SOURCES shader_count)
math(EXPR last_shader "${shader_count} - 1")
foreach(index RANGE 0 ${last_shader})
    list(GET HW3D_SHADER_SOURCES ${index} source)
    list(GET HW3D_SHADER_SYMBOLS ${index} symbol)
    list(GET HW3D_SHADER_STAGES ${index} stage)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/${symbol}.spv")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/generated"
        COMMAND "${HW3D_GLSLC}" --target-env=vulkan1.4
            "-fshader-stage=${stage}"
            -O "${source}" -o "${output}"
        DEPENDS "${source}"
        COMMENT "Compiling ${source} to SPIR-V"
        VERBATIM
    )
    if(HW3D_SPIRV_VAL)
        add_custom_command(
            OUTPUT "${output}.validated"
            COMMAND "${HW3D_SPIRV_VAL}" --target-env vulkan1.4 "${output}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${output}.validated"
            DEPENDS "${output}"
            COMMENT "Validating ${output}"
            VERBATIM
        )
        list(APPEND HW3D_SPIRV_VALIDATION_FILES "${output}.validated")
    endif()
    list(APPEND HW3D_SPIRV_FILES "${output}")
endforeach()

set(HW3D_SHADER_HEADER
    "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_spirv.hpp")
string(JOIN "|" HW3D_SPIRV_FILES_ARGUMENT ${HW3D_SPIRV_FILES})
string(JOIN "|" HW3D_SHADER_SYMBOLS_ARGUMENT ${HW3D_SHADER_SYMBOLS})
add_custom_command(
    OUTPUT "${HW3D_SHADER_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
        "-DSPIRV_FILES=${HW3D_SPIRV_FILES_ARGUMENT}"
        "-DSYMBOLS=${HW3D_SHADER_SYMBOLS_ARGUMENT}"
        "-DOUTPUT_HEADER=${HW3D_SHADER_HEADER}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
    DEPENDS
        ${HW3D_SPIRV_FILES}
        ${HW3D_SPIRV_VALIDATION_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spirv.cmake"
    COMMENT "Embedding SPIR-V"
    VERBATIM
)
add_custom_target(hw3d_spirv DEPENDS "${HW3D_SHADER_HEADER}")

add_library(hw3d_modules STATIC)
add_dependencies(hw3d_modules hw3d_spirv)
target_sources(
    hw3d_modules
    PRIVATE
        src/configuration/configuration.cpp
        src/geometry/geometry.cpp
        src/window_context/window_context.cpp
        src/gpu_pipeline/gpu_pipeline.cpp
        "${HW3D_SHADER_HEADER}"
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
            src/configuration/configuration.cppm
            src/geometry/geometry.cppm
            src/window_context/window_context.cppm
            src/gpu_pipeline/gpu_pipeline.cppm
)
target_include_directories(
    hw3d_modules
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(
    hw3d_modules
    PRIVATE Vulkan::Headers hw3d_vulkan_loader glfw)
target_compile_definitions(
    hw3d_modules
    PRIVATE
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
        $<$<CONFIG:Debug>:HW3D_DEBUG_VALIDATION=1>)

add_executable(hw3d src/main.cpp)
target_link_libraries(hw3d PRIVATE hw3d_modules)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    function(add_hw3d_test target source)
        add_executable(${target} ${source})
        target_link_libraries(
            ${target} PRIVATE hw3d_modules GTest::gtest_main)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES LABELS unit)
    endfunction()

    add_hw3d_test(configuration_tests tests/configuration_tests.cpp)
    add_hw3d_test(geometry_tests tests/geometry_tests.cpp)

    add_test(
        NAME cli_e2e
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_e2e.py"
            --binary "$<TARGET_FILE:hw3d>")
    set_tests_properties(
        cli_e2e
        PROPERTIES
            LABELS e2e
            TIMEOUT 30
            ENVIRONMENT_MODIFICATION
                "DISPLAY=unset:;WAYLAND_DISPLAY=unset:")

    find_program(HW3D_XVFB_RUN_EXECUTABLE xvfb-run)
    set(windowed_command
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/windowed_e2e.py"
        --binary "$<TARGET_FILE:hw3d>")
    if(HW3D_XVFB_RUN_EXECUTABLE AND "$ENV{DISPLAY}" STREQUAL "")
        list(PREPEND windowed_command "${HW3D_XVFB_RUN_EXECUTABLE}" -a)
    endif()
    add_test(NAME windowed_vulkan_e2e COMMAND ${windowed_command})
    set_tests_properties(
        windowed_vulkan_e2e
        PROPERTIES
            LABELS e2e
            RUN_SERIAL TRUE
            TIMEOUT 60
            ENVIRONMENT_MODIFICATION
                "WAYLAND_DISPLAY=unset:;HW3D_DISABLE_VALIDATION=set:1")

    set(generator_command
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/generate_triangles_tests.py"
        --generator "${CMAKE_CURRENT_SOURCE_DIR}/generate_triangles.py"
        --binary "$<TARGET_FILE:hw3d>")
    if(HW3D_XVFB_RUN_EXECUTABLE AND "$ENV{DISPLAY}" STREQUAL "")
        list(PREPEND generator_command "${HW3D_XVFB_RUN_EXECUTABLE}" -a)
    endif()
    add_test(NAME generate_triangles_e2e COMMAND ${generator_command})
    set_tests_properties(
        generate_triangles_e2e
        PROPERTIES
            LABELS e2e
            RUN_SERIAL TRUE
            TIMEOUT 60
            ENVIRONMENT_MODIFICATION
                "WAYLAND_DISPLAY=unset:;HW3D_DISABLE_VALIDATION=set:1")
endif()
