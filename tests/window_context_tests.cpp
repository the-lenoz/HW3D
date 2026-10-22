#include <gtest/gtest.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

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

TEST(WindowContext, UpdatesPerformanceTitleUsingMeasuredInterval)
{
    hw3d::WindowContext window{64, 64, "custom title"};
    int calls = 0;
    glfwSetTime(0.0);

    window.run([&](const hw3d::FrameInput&) {
        ++calls;
        if (calls == 1) {
            glfwSetTime(2.0);
            return;
        }

#if GLFW_VERSION_MAJOR > 3 \
    || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
        const char* const raw_title = glfwGetWindowTitle(
            glfwGetCurrentContext());
        ASSERT_NE(raw_title, nullptr);
        const std::string title{raw_title};
        EXPECT_EQ(title.rfind("custom title | FPS: ", 0), 0U);

        double fps{};
        double milliseconds_per_frame{};
        ASSERT_EQ(
            std::sscanf(
                title.c_str(),
                "custom title | FPS: %lf (%lf ms/frame)",
                &fps,
                &milliseconds_per_frame),
            2);
        EXPECT_NEAR(fps * milliseconds_per_frame, 1000.0, 1.0);
#endif
        window.request_close();
    });

    EXPECT_EQ(calls, 2);
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
