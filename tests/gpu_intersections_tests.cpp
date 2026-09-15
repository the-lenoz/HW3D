#include <gtest/gtest.h>

#include <glad/gl.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>

import hw3d.gpu_intersections;
import hw3d.window_context;

namespace {

TEST(GpuIntersections, PlaceholderComputeShaderCompiles)
{
    hw3d::WindowContext window{64, 64, "GPU intersections test"};
    std::ifstream input{HW3D_GPU_SHADER_PATH};
    ASSERT_TRUE(input.is_open());

    const std::string source{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    ASSERT_FALSE(source.empty());

    const GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    ASSERT_NE(shader, 0U);
    const char* const source_pointer = source.c_str();
    glShaderSource(shader, 1, &source_pointer, nullptr);
    glCompileShader(shader);

    GLint compiled{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    GLint log_length{};
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    std::string log(
        static_cast<std::size_t>(std::max(log_length, 1)),
        '\0');
    glGetShaderInfoLog(shader, log_length, nullptr, log.data());

    EXPECT_EQ(compiled, GL_TRUE) << log;
    glDeleteShader(shader);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

}
