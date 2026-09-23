module;

#include <functional>
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
};

class WindowContext final {
public:
    WindowContext(
        int width,
        int height,
        const char* title,
        bool headless = false);
    ~WindowContext();

    WindowContext(const WindowContext&) = delete;
    WindowContext& operator=(const WindowContext&) = delete;
    WindowContext(WindowContext&&) = delete;
    WindowContext& operator=(WindowContext&&) = delete;

    void run(std::function<void(const FrameInput&)> on_frame = {});
    void request_close() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
