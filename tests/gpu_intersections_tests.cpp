#include <gtest/gtest.h>

#include <glad/gl.h>
#include "embedded_shaders.hpp"

import hw3d.gpu_intersections;
import hw3d.shader_program;
import hw3d.window_context;

namespace {

TEST(GpuIntersections, EmbeddedComputeShaderLinksThroughSharedProgram)
{
    hw3d::WindowContext window{64, 64, "GPU intersections test"};
    const auto program = hw3d::ShaderProgram::compute(
        hw3d::shaders::intersections_comp_glsl);

    program.use();
    GLint bound_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &bound_program);
    EXPECT_GT(bound_program, 0);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    glUseProgram(0);
}

}
