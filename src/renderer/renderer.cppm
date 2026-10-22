module;

#include <memory>
#include <span>

export module hw3d.renderer;

import hw3d.geometry;

export namespace hw3d {

class Renderer final {
public:
    Renderer(
        std::span<const Triangle> triangles,
        std::span<const Aabb> motion_bounds);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void render();
    void move_forward(float delta_seconds) noexcept;
    void move_back(float delta_seconds) noexcept;
    void move_left(float delta_seconds) noexcept;
    void move_right(float delta_seconds) noexcept;
    void rotate(float x_offset, float y_offset) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
