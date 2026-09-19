if(NOT DEFINED VERTEX_SHADER OR NOT DEFINED FRAGMENT_SHADER
        OR NOT DEFINED COMPUTE_SHADER
        OR NOT DEFINED OUTPUT_HEADER)
    message(FATAL_ERROR "Shader inputs and output header are required")
endif()

set(header "#pragma once\n\nnamespace hw3d::shaders {\n\n")

foreach(shader_file IN ITEMS
        "${VERTEX_SHADER}" "${FRAGMENT_SHADER}" "${COMPUTE_SHADER}")
    get_filename_component(shader_name "${shader_file}" NAME)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" symbol "${shader_name}")
    file(READ "${shader_file}" source)

    set(salt 0)
    while(TRUE)
        string(SHA256 digest "${source}${salt}")
        string(SUBSTRING "${digest}" 0 10 suffix)
        set(delimiter "HW3D_${suffix}")
        string(FIND "${source}" ")${delimiter}\"" delimiter_position)
        if(delimiter_position EQUAL -1)
            break()
        endif()
        math(EXPR salt "${salt} + 1")
    endwhile()

    string(APPEND header
        "inline constexpr char ${symbol}[] = R\"${delimiter}(${source})${delimiter}\";\n\n")
endforeach()

string(APPEND header "} // namespace hw3d::shaders\n")
get_filename_component(output_directory "${OUTPUT_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(WRITE "${OUTPUT_HEADER}" "${header}")
