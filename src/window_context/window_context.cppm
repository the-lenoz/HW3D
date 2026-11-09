module;

#include <functional>
#include <cstdint>
#include <memory>

export module hw3d.window_context;

export namespace hw3d {

struct FrameInput {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    float mouse_x_offset = 0.0F;
    float mouse_y_offset = 0.0F;
    float delta_seconds = 0.0F;
    std::uint32_t framebuffer_width = 0;
    std::uint32_t framebuffer_height = 0;
};

class WindowContext final {
public:
    WindowContext(int width, int height, const char* title);
    ~WindowContext();

    WindowContext(const WindowContext&) = delete;
    WindowContext& operator=(const WindowContext&) = delete;
    WindowContext(WindowContext&&) = delete;
    WindowContext& operator=(WindowContext&&) = delete;

    void run(std::function<void(const FrameInput&)> on_frame = {});
    void request_close() noexcept;

    // Opaque GLFW handle for the Vulkan integration layer. No GLFW or
    // Vulkan type leaks through the public module boundary.
    [[nodiscard]] void* native_handle() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
