module;

#include <memory>

export module hw3d.renderer;

import hw3d.configuration;

export namespace hw3d {

class Renderer final {
public:
    explicit Renderer(const Configuration& configuration);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void render() noexcept;
    void move_forward() noexcept;
    void move_back() noexcept;
    void move_left() noexcept;
    void move_right() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

void render_frame(void* renderer) noexcept;
void move_camera_forward(void* renderer) noexcept;
void move_camera_back(void* renderer) noexcept;
void move_camera_left(void* renderer) noexcept;
void move_camera_right(void* renderer) noexcept;

}
