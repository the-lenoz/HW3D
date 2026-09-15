module;

#include <memory>

export module hw3d.window_context;

export namespace hw3d {

enum class ArrowKey {
    up,
    down,
    left,
    right,
};

using WindowCallbackFunction = void (*)(void*) noexcept;
using MovementCallbackFunction = void (*)(void*, float) noexcept;
using MouseMoveCallbackFunction = void (*)(void*, float, float) noexcept;

struct WindowCallback {
    WindowCallbackFunction function = nullptr;
    void* context = nullptr;
};

struct MovementCallback {
    MovementCallbackFunction function = nullptr;
    void* context = nullptr;
};

struct MouseMoveCallback {
    MouseMoveCallbackFunction function = nullptr;
    void* context = nullptr;
};

class WindowContext final {
public:
    WindowContext(int width, int height, const char* title);
    ~WindowContext();

    WindowContext(const WindowContext&) = delete;
    WindowContext& operator=(const WindowContext&) = delete;
    WindowContext(WindowContext&&) = delete;
    WindowContext& operator=(WindowContext&&) = delete;

    void register_arrow_callback(
        ArrowKey key,
        MovementCallback callback) noexcept;
    void register_mouse_move_callback(MouseMoveCallback callback) noexcept;

    void run(WindowCallback frame_callback = {});
    void request_close() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
