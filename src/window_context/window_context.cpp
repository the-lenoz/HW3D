module;

#define GLFW_INCLUDE_NONE
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

module hw3d.window_context;

namespace hw3d {
namespace {

bool window_context_exists = false;

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

        if (gladLoadGL(
                reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
            window_context_exists = false;
            glfwTerminate();
            throw std::runtime_error("failed to initialize GLAD");
        }

        glfwSwapInterval(1);
        glfwSetWindowUserPointer(window_, this);
        glfwSetKeyCallback(window_, &Impl::on_key);
        glfwSetCursorPosCallback(window_, &Impl::on_cursor_position);
        glfwSetFramebufferSizeCallback(window_, &Impl::on_framebuffer_size);
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
            glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }

        int framebuffer_width{};
        int framebuffer_height{};
        glfwGetFramebufferSize(
            window_,
            &framebuffer_width,
            &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
    }

    ~Impl()
    {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
        window_context_exists = false;
    }

    void run(const std::function<void(const FrameInput&)>& on_frame)
    {
        double previous_time = glfwGetTime();

        while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
            glfwPollEvents();
            if (glfwWindowShouldClose(window_) == GLFW_TRUE) {
                break;
            }

            const double current_time = glfwGetTime();
            const float delta_seconds = static_cast<float>(std::clamp(
                current_time - previous_time,
                0.0,
                0.1));
            previous_time = current_time;

            const FrameInput input{
                .up = glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS,
                .down = glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS,
                .left = glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS,
                .right = glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS,
                .mouse_x_offset = mouse_x_offset_,
                .mouse_y_offset = mouse_y_offset_,
                .delta_seconds = delta_seconds,
            };
            mouse_x_offset_ = 0.0F;
            mouse_y_offset_ = 0.0F;

            if (on_frame) {
                on_frame(input);
            }

            glfwSwapBuffers(window_);
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
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    static void on_cursor_position(
        GLFWwindow* const window,
        const double x_position,
        const double y_position) noexcept
    {
        auto* const self = static_cast<Impl*>(
            glfwGetWindowUserPointer(window));
        if (self == nullptr) {
            return;
        }

        if (self->first_mouse_event_) {
            self->last_cursor_x_ = x_position;
            self->last_cursor_y_ = y_position;
            self->first_mouse_event_ = false;
            return;
        }

        self->mouse_x_offset_ += static_cast<float>(
            x_position - self->last_cursor_x_);
        self->mouse_y_offset_ += static_cast<float>(
            self->last_cursor_y_ - y_position);
        self->last_cursor_x_ = x_position;
        self->last_cursor_y_ = y_position;
    }

    static void on_framebuffer_size(
        GLFWwindow*,
        const int width,
        const int height) noexcept
    {
        glViewport(0, 0, width, height);
    }

    GLFWwindow* window_ = nullptr;
    double last_cursor_x_{};
    double last_cursor_y_{};
    float mouse_x_offset_{};
    float mouse_y_offset_{};
    bool first_mouse_event_ = true;
};

WindowContext::WindowContext(
    const int width,
    const int height,
    const char* const title)
    : impl_(std::make_unique<Impl>(width, height, title))
{
}

WindowContext::~WindowContext() = default;

void WindowContext::run(std::function<void(const FrameInput&)> on_frame)
{
    impl_->run(on_frame);
}

void WindowContext::request_close() noexcept
{
    impl_->request_close();
}

}
