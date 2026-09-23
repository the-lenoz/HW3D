#include <gtest/gtest.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cmath>
#include <stdexcept>

import hw3d.window_context;

namespace {

TEST(WindowContext, RejectsNonPositiveWidth)
{
    EXPECT_THROW(
        hw3d::WindowContext(0, 64, "test"),
        std::invalid_argument);
    EXPECT_THROW(
        hw3d::WindowContext(-1, 64, "test"),
        std::invalid_argument);
}

TEST(WindowContext, RejectsNonPositiveHeight)
{
    EXPECT_THROW(
        hw3d::WindowContext(64, 0, "test"),
        std::invalid_argument);
    EXPECT_THROW(
        hw3d::WindowContext(64, -1, "test"),
        std::invalid_argument);
}

TEST(WindowContext, RejectsNullTitle)
{
    EXPECT_THROW(
        hw3d::WindowContext(64, 64, nullptr),
        std::invalid_argument);
}

TEST(WindowContext, RejectsSecondLiveInstance)
{
    hw3d::WindowContext first{64, 64, "first"};

    EXPECT_THROW(
        hw3d::WindowContext(64, 64, "second"),
        std::logic_error);
}

TEST(WindowContext, CanBeCreatedAgainAfterDestruction)
{
    {
        hw3d::WindowContext first{64, 64, "first"};
        first.request_close();
    }

    hw3d::WindowContext second{64, 64, "second"};
    second.request_close();
}

TEST(WindowContext, HeadlessContextIsHiddenAndCanBeFollowedByVisibleWindow)
{
    {
        hw3d::WindowContext hidden{64, 64, "hidden", true};
        GLFWwindow* const current = glfwGetCurrentContext();

        ASSERT_NE(current, nullptr);
        EXPECT_EQ(
            glfwGetWindowAttrib(current, GLFW_VISIBLE),
            GLFW_FALSE);
    }

    hw3d::WindowContext visible{64, 64, "visible"};
    GLFWwindow* const current = glfwGetCurrentContext();

    ASSERT_NE(current, nullptr);
    EXPECT_EQ(glfwGetWindowAttrib(current, GLFW_VISIBLE), GLFW_TRUE);
    visible.request_close();
}

TEST(WindowContext, RequestCloseStopsBeforeFrameCallback)
{
    hw3d::WindowContext window{64, 64, "close"};
    int calls = 0;

    window.request_close();
    window.run([&calls](const hw3d::FrameInput&) { ++calls; });

    EXPECT_EQ(calls, 0);
}

TEST(WindowContext, RunProvidesOneFrameInput)
{
    hw3d::WindowContext window{64, 64, "frame input"};
    int calls = 0;

    window.run([&](const hw3d::FrameInput& input) {
        ++calls;
        EXPECT_FALSE(input.up);
        EXPECT_FALSE(input.down);
        EXPECT_FALSE(input.left);
        EXPECT_FALSE(input.right);
        EXPECT_TRUE(std::isfinite(input.mouse_x_offset));
        EXPECT_TRUE(std::isfinite(input.mouse_y_offset));
        EXPECT_GE(input.delta_seconds, 0.0F);
        EXPECT_LE(input.delta_seconds, 0.1F);
        window.request_close();
    });

    EXPECT_EQ(calls, 1);
}

TEST(WindowContext, AcceptsEmptyFrameCallback)
{
    hw3d::WindowContext window{64, 64, "empty frame callback"};

    window.request_close();

    EXPECT_NO_THROW(window.run());
}

TEST(WindowContext, PropagatesFrameCallbackException)
{
    hw3d::WindowContext window{64, 64, "throwing frame callback"};

    EXPECT_THROW(
        window.run([](const hw3d::FrameInput&) {
            throw std::runtime_error("frame failure");
        }),
        std::runtime_error);
}

}
