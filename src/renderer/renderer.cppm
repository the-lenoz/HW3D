module;

#include <memory>
#include <vector>

export module hw3d.renderer;

import hw3d.configuration;

export namespace hw3d {

class Renderer final {
public:
    Renderer(
        const std::vector<Triangle>& triangles,
        const std::vector<bool>& highlighted);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void render() noexcept;
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
