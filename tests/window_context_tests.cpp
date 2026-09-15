#include <gtest/gtest.h>

#include <stdexcept>

import hw3d.window_context;

namespace {

struct FrameState {
    hw3d::WindowContext* window;
    int calls;
};

void close_after_frame(void* const context) noexcept
{
    auto& state = *static_cast<FrameState*>(context);
    ++state.calls;
    state.window->request_close();
}

void movement_callback(void*, float) noexcept
{
}

void mouse_callback(void*, float, float) noexcept
{
}

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

TEST(WindowContext, RequestCloseStopsBeforeFrameCallback)
{
    hw3d::WindowContext window{64, 64, "close"};
    FrameState state{&window, 0};

    window.request_close();
    window.run({&close_after_frame, &state});

    EXPECT_EQ(state.calls, 0);
}

TEST(WindowContext, RunForwardsContextToFrameCallback)
{
    hw3d::WindowContext window{64, 64, "frame callback"};
    FrameState state{&window, 0};

    window.run({&close_after_frame, &state});

    EXPECT_EQ(state.calls, 1);
}

TEST(WindowContext, AcceptsEmptyCallbacksForEveryInputSlot)
{
    hw3d::WindowContext window{64, 64, "empty callbacks"};

    window.register_arrow_callback(hw3d::ArrowKey::up, {});
    window.register_arrow_callback(hw3d::ArrowKey::down, {});
    window.register_arrow_callback(hw3d::ArrowKey::left, {});
    window.register_arrow_callback(hw3d::ArrowKey::right, {});
    window.register_mouse_move_callback({});
    window.request_close();

    EXPECT_NO_THROW(window.run());
}

TEST(WindowContext, AcceptsPopulatedCallbacksForEveryInputSlot)
{
    hw3d::WindowContext window{64, 64, "populated callbacks"};

    window.register_arrow_callback(
        hw3d::ArrowKey::up,
        {&movement_callback, nullptr});
    window.register_arrow_callback(
        hw3d::ArrowKey::down,
        {&movement_callback, nullptr});
    window.register_arrow_callback(
        hw3d::ArrowKey::left,
        {&movement_callback, nullptr});
    window.register_arrow_callback(
        hw3d::ArrowKey::right,
        {&movement_callback, nullptr});
    window.register_mouse_move_callback({&mouse_callback, nullptr});
    window.request_close();

    EXPECT_NO_THROW(window.run());
}

}
