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

using WindowCallback = void (*)() noexcept;

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
        WindowCallback callback) noexcept;

    void run(WindowCallback frame_callback = nullptr);
    void request_close() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
