#include <gtest/gtest.h>

#include <glad/gl.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

import hw3d.shader_program;
import hw3d.window_context;

namespace {

constexpr char vertex_source[] = R"(
#version 460 core
uniform float offset;
out vec3 value;
void main() {
    value = vec3(1.0);
    gl_Position = vec4(offset, 0.0, 0.0, 1.0);
}
)";

constexpr char fragment_source[] = R"(
#version 460 core
in vec3 value;
out vec4 color;
void main() { color = vec4(value, 1.0); }
)";

constexpr char compute_source[] = R"(
#version 460 core
layout(local_size_x=1) in;
void main() {}
)";

class ShaderProgramTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        window_ = std::make_unique<hw3d::WindowContext>(
            64, 64, "shader program tests");
    }

    static void TearDownTestSuite()
    {
        window_.reset();
    }

    void TearDown() override
    {
        glUseProgram(0);
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
    }

    static inline std::unique_ptr<hw3d::WindowContext> window_;
};

TEST_F(ShaderProgramTest, CreatesAndUsesGraphicsProgram)
{
    const auto program = hw3d::ShaderProgram::graphics(
        vertex_source, fragment_source);
    EXPECT_GE(program.uniform_location("offset"), 0);
    EXPECT_EQ(program.uniform_location("missing"), -1);

    program.use();
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    EXPECT_GT(current, 0);
}

TEST_F(ShaderProgramTest, CreatesAndUsesComputeProgram)
{
    const auto program = hw3d::ShaderProgram::compute(compute_source);
    program.use();
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    EXPECT_GT(current, 0);
}

TEST_F(ShaderProgramTest, ReportsStageOnCompilationFailure)
{
    try {
        (void)hw3d::ShaderProgram::graphics("invalid GLSL", fragment_source);
        FAIL() << "vertex compilation unexpectedly succeeded";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("vertex shader compilation failed"),
                  std::string::npos);
    }

    try {
        (void)hw3d::ShaderProgram::compute("invalid GLSL");
        FAIL() << "compute compilation unexpectedly succeeded";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("compute shader compilation failed"),
                  std::string::npos);
    }
}

TEST_F(ShaderProgramTest, ReportsLinkFailure)
{
    constexpr char mismatched_fragment[] = R"(
#version 460 core
in vec4 value;
out vec4 color;
void main() { color = value; }
)";

    try {
        (void)hw3d::ShaderProgram::graphics(
            vertex_source, mismatched_fragment);
        FAIL() << "link unexpectedly succeeded";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("linking failed"),
                  std::string::npos);
    }
}

TEST_F(ShaderProgramTest, MoveTransfersOwnership)
{
    auto first = hw3d::ShaderProgram::compute(compute_source);
    first.use();
    GLint first_handle = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &first_handle);

    auto second = std::move(first);
    second.use();
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    EXPECT_EQ(current, first_handle);

    auto third = hw3d::ShaderProgram::compute(compute_source);
    third.use();
    GLint old_third_handle = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_third_handle);
    glUseProgram(0);

    third = std::move(second);
    EXPECT_EQ(glIsProgram(static_cast<GLuint>(old_third_handle)), GL_FALSE);
    third.use();
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    EXPECT_EQ(current, first_handle);
}

}
