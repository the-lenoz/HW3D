#include <gtest/gtest.h>

#include <glad/gl.h>

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

import hw3d.configuration;
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
    const std::vector<hw3d::Triangle>& triangles,
    const bool highlighted)
{
    hw3d::Renderer renderer{triangles, {highlighted}};
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
    EXPECT_THROW(
        hw3d::Renderer({}, {}),
        std::invalid_argument);
}

TEST_F(RendererTest, RejectsMismatchedHighlightArray)
{
    EXPECT_THROW(
        hw3d::Renderer(centered_triangle, {}),
        std::invalid_argument);
    EXPECT_THROW(
        hw3d::Renderer(centered_triangle, {false, true}),
        std::invalid_argument);
}

TEST_F(RendererTest, ConfiguresDepthTestAndRendersGrayTriangle)
{
    hw3d::Renderer renderer{centered_triangle, {false}};

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
    hw3d::Renderer renderer{centered_triangle, {true}};

    renderer.render();
    glFinish();
    const auto pixel = center_pixel();

    EXPECT_GT(pixel[0], pixel[1] + 60U);
    EXPECT_GT(pixel[0], pixel[2] + 60U);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
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
    hw3d::Renderer renderer{centered_triangle, {false}};

    renderer.move_forward(0.1F);
    renderer.move_back(0.1F);
    renderer.move_left(0.1F);
    renderer.move_right(0.1F);
    renderer.rotate(250.0F, 100000.0F);
    renderer.rotate(-250.0F, -100000.0F);
    renderer.render();

    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, CallbackAdaptersInvokeEveryRendererOperation)
{
    hw3d::Renderer renderer{centered_triangle, {false}};

    hw3d::move_camera_forward(&renderer, 0.1F);
    hw3d::move_camera_back(&renderer, 0.1F);
    hw3d::move_camera_left(&renderer, 0.1F);
    hw3d::move_camera_right(&renderer, 0.1F);
    hw3d::rotate_camera(&renderer, 10.0F, -10.0F);
    hw3d::render_frame(&renderer);

    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, CallbackAdaptersIgnoreNullContext)
{
    hw3d::move_camera_forward(nullptr, 0.1F);
    hw3d::move_camera_back(nullptr, 0.1F);
    hw3d::move_camera_left(nullptr, 0.1F);
    hw3d::move_camera_right(nullptr, 0.1F);
    hw3d::rotate_camera(nullptr, 1.0F, 1.0F);
    hw3d::render_frame(nullptr);

    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RendererTest, RenderSkipsZeroSizedViewport)
{
    hw3d::Renderer renderer{centered_triangle, {false}};
    glViewport(0, 0, 0, 0);

    renderer.render();

    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    glViewport(0, 0, framebuffer_size, framebuffer_size);
}

}
