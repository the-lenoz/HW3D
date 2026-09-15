module;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

module hw3d.window_context;

namespace hw3d {
namespace {

bool window_context_exists = false;

constexpr std::size_t arrow_index(const ArrowKey key) noexcept
{
    switch (key) {
    case ArrowKey::up:
        return 0;
    case ArrowKey::down:
        return 1;
    case ArrowKey::left:
        return 2;
    case ArrowKey::right:
        return 3;
    }

    return 0;
}

}

class WindowContext::Impl final {
public:
    Impl(const int width, const int height, const char* const title)
    {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("window dimensions must be positive");
        }
        if (title == nullptr) {
            throw std::invalid_argument("window title must not be null");
        }
        if (window_context_exists) {
            throw std::logic_error(
                "only one WindowContext may exist at a time");
        }
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("failed to initialize GLFW");
        }

        window_context_exists = true;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

        window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (window_ == nullptr) {
            const char* description = nullptr;
            glfwGetError(&description);
            const std::string message = description == nullptr
                ? "failed to create GLFW window"
                : "failed to create GLFW window: " + std::string(description);

            window_context_exists = false;
            glfwTerminate();
            throw std::runtime_error(message);
        }

        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(window_, this);
        glfwSetKeyCallback(window_, &Impl::on_key);
    }

    ~Impl()
    {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
        window_context_exists = false;
    }

    void register_arrow_callback(
        const ArrowKey key,
        const WindowCallback callback) noexcept
    {
        arrow_callbacks_[arrow_index(key)] = callback;
    }

    void run(const WindowCallback frame_callback)
    {
        while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
            if (frame_callback != nullptr) {
                frame_callback();
            }

            glfwSwapBuffers(window_);
            glfwPollEvents();
        }
    }

    void request_close() noexcept
    {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

private:
    static void on_key(
        GLFWwindow* const window,
        const int key,
        int,
        const int action,
        int) noexcept
    {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) {
            return;
        }

        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }

        auto* const self = static_cast<Impl*>(
            glfwGetWindowUserPointer(window));
        if (self == nullptr) {
            return;
        }

        switch (key) {
        case GLFW_KEY_UP:
            self->invoke(ArrowKey::up);
            break;
        case GLFW_KEY_DOWN:
            self->invoke(ArrowKey::down);
            break;
        case GLFW_KEY_LEFT:
            self->invoke(ArrowKey::left);
            break;
        case GLFW_KEY_RIGHT:
            self->invoke(ArrowKey::right);
            break;
        default:
            break;
        }
    }

    void invoke(const ArrowKey key) noexcept
    {
        const WindowCallback callback = arrow_callbacks_[arrow_index(key)];
        if (callback != nullptr) {
            callback();
        }
    }

    GLFWwindow* window_ = nullptr;
    std::array<WindowCallback, 4> arrow_callbacks_{};
};

WindowContext::WindowContext(
    const int width,
    const int height,
    const char* const title)
    : impl_(std::make_unique<Impl>(width, height, title))
{
}

WindowContext::~WindowContext() = default;

void WindowContext::register_arrow_callback(
    const ArrowKey key,
    const WindowCallback callback) noexcept
{
    impl_->register_arrow_callback(key, callback);
}

void WindowContext::run(const WindowCallback frame_callback)
{
    impl_->run(frame_callback);
}

void WindowContext::request_close() noexcept
{
    impl_->request_close();
}

}
