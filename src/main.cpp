#include <exception>
#include <iostream>
#include <vector>

import hw3d.configuration;
import hw3d.window_context;
import hw3d.renderer;
import hw3d.cpu_intersections;
import hw3d.gpu_intersections;

int main()
{
    try {
        const auto configuration = hw3d::read_configuration(std::cin);
        const std::vector<bool> highlighted =
            hw3d::cpu_collisions(configuration.triangles);

        hw3d::WindowContext window{1280, 720, "HW3D"};
        hw3d::Renderer renderer{configuration.triangles, highlighted};

        window.register_arrow_callback(
            hw3d::ArrowKey::up,
            {&hw3d::move_camera_forward, &renderer});
        window.register_arrow_callback(
            hw3d::ArrowKey::down,
            {&hw3d::move_camera_back, &renderer});
        window.register_arrow_callback(
            hw3d::ArrowKey::left,
            {&hw3d::move_camera_left, &renderer});
        window.register_arrow_callback(
            hw3d::ArrowKey::right,
            {&hw3d::move_camera_right, &renderer});
        window.register_mouse_move_callback(
            {&hw3d::rotate_camera, &renderer});

        window.run({&hw3d::render_frame, &renderer});
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
