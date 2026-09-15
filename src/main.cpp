#include <exception>
#include <iostream>

import hw3d.configuration;
import hw3d.window_context;
import hw3d.renderer;
import hw3d.cpu_intersections;
import hw3d.gpu_intersections;

int main()
{
    try {
        const auto configuration = hw3d::read_configuration(std::cin);
        static_cast<void>(configuration);

        hw3d::WindowContext window{1280, 720, "HW3D"};
        window.register_arrow_callback(
            hw3d::ArrowKey::up,
            &hw3d::move_camera_up);
        window.register_arrow_callback(
            hw3d::ArrowKey::down,
            &hw3d::move_camera_down);
        window.register_arrow_callback(
            hw3d::ArrowKey::left,
            &hw3d::move_camera_left);
        window.register_arrow_callback(
            hw3d::ArrowKey::right,
            &hw3d::move_camera_right);

        window.run(&hw3d::render_frame);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
