#include <gtest/gtest.h>

#include <glad/gl.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

import hw3d.configuration;
import hw3d.geometry;
import hw3d.gpu_triangles;
import hw3d.renderer;
import hw3d.window_context;

namespace {

constexpr int framebuffer_size = 96;

const std::vector<hw3d::Triangle> centered_triangle{{
    {-1.0F, -1.0F, 0.0F},
    {1.0F, -1.0F, 0.0F},
    {0.0F, 1.0F, 0.0F},
}};

const std::vector<hw3d::Triangle> light_facing_triangle{{
    {-1.0F, -1.0F, 1.0F},
    {1.0F, -1.0F, 1.0F},
    {0.0F, 1.0F, -1.0F},
}};

const std::vector<hw3d::Triangle> light_perpendicular_triangle{{
    {0.0F, -1.0F, 0.0F},
    {0.0F, 1.0F, 0.0F},
    {-0.35F, 0.0F, 0.45F},
}};

std::vector<hw3d::Aabb> static_motion_bounds(
    const std::vector<hw3d::Triangle>& triangles)
{
    std::vector<hw3d::Aabb> result;
    result.reserve(triangles.size());
    for (const hw3d::Triangle& triangle : triangles) {
        result.push_back(hw3d::aabb(triangle));
    }
    return result;
}

void clear_gl_errors() noexcept
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

std::array<std::uint8_t, 3> center_pixel()
{
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);

    std::array<std::uint8_t, 3> pixel{};
    glReadBuffer(GL_BACK);
    glReadPixels(
        viewport[0] + viewport[2] / 2,
        viewport[1] + viewport[3] / 2,
        1,
        1,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        pixel.data());
    return pixel;
}

std::array<std::uint8_t, 3> rendered_center_pixel(
    std::vector<hw3d::Triangle> triangles,
    const bool highlighted)
{
    for (hw3d::Triangle& triangle : triangles) {
        triangle.highlighted = highlighted;
    }
    const auto bounds = static_motion_bounds(triangles);
    hw3d::Renderer renderer{triangles, bounds};
    renderer.render();
    glFinish();
    return center_pixel();
}

class RendererTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        window_ = std::make_unique<hw3d::WindowContext>(
            framebuffer_size,
            framebuffer_size,
            "renderer tests");
    }

    static void TearDownTestSuite()
    {
        window_.reset();
    }

    void SetUp() override
    {
        glViewport(0, 0, framebuffer_size, framebuffer_size);
        clear_gl_errors();
    }

    static inline std::unique_ptr<hw3d::WindowContext> window_;
};

TEST_F(RendererTest, RejectsEmptyTriangleArray)
{
    std::vector<hw3d::Triangle> triangles;
    const std::vector<hw3d::Aabb> bounds;
    EXPECT_THROW(
        static_cast<void>(hw3d::Renderer{triangles, bounds}),
        std::invalid_argument);
}

TEST_F(RendererTest, ReadsHighlightFromTriangle)
{
    auto triangles = centered_triangle;
    triangles.front().highlighted = true;
    const auto bounds = static_motion_bounds(triangles);
    EXPECT_NO_THROW({ hw3d::Renderer renderer(triangles, bounds); });
}

TEST_F(RendererTest, RejectsMismatchedMotionBounds)
{
    auto triangles = centered_triangle;
    const std::vector<hw3d::Aabb> bounds;

    EXPECT_THROW(
        static_cast<void>(hw3d::Renderer{triangles, bounds}),
        std::invalid_argument);
}

TEST_F(RendererTest, ConfiguresDepthTestAndRendersGrayTriangle)
{
    auto triangles = centered_triangle;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::Renderer renderer{triangles, bounds};

    renderer.render();
    glFinish();

    GLint depth_function{};
    glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
    const auto pixel = center_pixel();

    EXPECT_EQ(glIsEnabled(GL_DEPTH_TEST), GL_TRUE);
    EXPECT_EQ(depth_function, GL_LESS);
    EXPECT_GT(pixel[0], 40U);
    EXPECT_NEAR(pixel[0], pixel[1], 3);
    EXPECT_NEAR(pixel[1], pixel[2], 3);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, RendersHighlightedTriangleRed)
{
    auto triangles = centered_triangle;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::Renderer renderer{triangles, bounds};

    renderer.render();
    glFinish();
    const auto regular_pixel = center_pixel();

    triangles.front().highlighted = true;
    renderer.render();
    glFinish();
    const auto highlighted_pixel = center_pixel();

    EXPECT_NEAR(regular_pixel[0], regular_pixel[1], 3);
    EXPECT_GT(highlighted_pixel[0], highlighted_pixel[1] + 60U);
    EXPECT_GT(highlighted_pixel[0], highlighted_pixel[2] + 60U);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, RendersDirectlyFromBorrowedGpuTriangleBuffer)
{
    auto triangles = centered_triangle;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::GpuTriangleBuffer buffer{triangles.size()};
    buffer.upload(triangles);
    hw3d::Renderer renderer{buffer, bounds};

    renderer.render();
    glFinish();
    const auto regular_pixel = center_pixel();

    triangles.front().highlighted = true;
    buffer.upload(triangles);
    renderer.render();
    glFinish();
    const auto highlighted_pixel = center_pixel();

    EXPECT_NEAR(regular_pixel[0], regular_pixel[1], 3);
    EXPECT_GT(highlighted_pixel[0], highlighted_pixel[1] + 60U);
    EXPECT_GT(highlighted_pixel[0], highlighted_pixel[2] + 60U);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, ReadsChangedVerticesFromOriginalVector)
{
    auto triangles = centered_triangle;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::Renderer renderer{triangles, bounds};

    renderer.render();
    glFinish();
    const auto original_pixel = center_pixel();

    triangles.front().a.x += 100.0F;
    triangles.front().b.x += 100.0F;
    triangles.front().c.x += 100.0F;
    renderer.render();
    glFinish();
    const auto moved_pixel = center_pixel();

    EXPECT_GT(original_pixel[0], 40U);
    EXPECT_LT(moved_pixel[0], 20U);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, AcceptsTriangleSpan)
{
    auto triangles = centered_triangle;
    const auto bounds = static_motion_bounds(triangles);
    const std::span<const hw3d::Triangle> triangle_span{triangles};

    EXPECT_NO_THROW(
        static_cast<void>(hw3d::Renderer{triangle_span, bounds}));
}

TEST_F(RendererTest, UsesDimAmbientAndStrongerDirectionalDiffuse)
{
    const auto ambient_pixel = rendered_center_pixel(
        light_perpendicular_triangle,
        false);
    const auto diffuse_pixel = rendered_center_pixel(
        light_facing_triangle,
        false);

    EXPECT_GT(ambient_pixel[0], 15U);
    EXPECT_LT(ambient_pixel[0], 40U);
    EXPECT_GT(diffuse_pixel[0], ambient_pixel[0] + 50U);
    EXPECT_NEAR(ambient_pixel[0], ambient_pixel[1], 3);
    EXPECT_NEAR(diffuse_pixel[0], diffuse_pixel[1], 3);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, MovementAndRotationMethodsRemainRenderable)
{
    auto triangles = centered_triangle;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::Renderer renderer{triangles, bounds};

    renderer.move_forward(0.1F);
    renderer.move_back(0.1F);
    renderer.move_left(0.1F);
    renderer.move_right(0.1F);
    renderer.rotate(250.0F, 100000.0F);
    renderer.rotate(-250.0F, -100000.0F);
    renderer.render();

    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, RenderSkipsZeroSizedViewport)
{
    auto triangles = centered_triangle;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::Renderer renderer{triangles, bounds};
    glViewport(0, 0, 0, 0);

    renderer.render();

    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    glViewport(0, 0, framebuffer_size, framebuffer_size);
}

}
