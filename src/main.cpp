#include <exception>
#include <iostream>
#include <vector>
#include <chrono>

import hw3d.configuration;
import hw3d.window_context;
import hw3d.renderer;
import hw3d.cpu_intersections;
import hw3d.gpu_intersections;

int main(const int argc, char **argv)
{
    try {
        hw3d::WindowContext window{1280, 720, "HW3D"};

        bool use_gpu_intersections = argc >= 2 && std::string(argv[1]) == "--use-gpu-intersections";

        const auto configuration = hw3d::read_configuration(std::cin);

        const std::vector<bool> highlighted =
            use_gpu_intersections ?
                hw3d::gpu_collisions(configuration.triangles):
                hw3d::cpu_collisions(configuration.triangles);

        hw3d::Renderer renderer{configuration.triangles, highlighted};

        window.run([&renderer](const hw3d::FrameInput& input) {
            renderer.rotate(input.mouse_x_offset, input.mouse_y_offset);
            if (input.up) {
                renderer.move_forward(input.delta_seconds);
            }
            if (input.down) {
                renderer.move_back(input.delta_seconds);
            }
            if (input.left) {
                renderer.move_left(input.delta_seconds);
            }
            if (input.right) {
                renderer.move_right(input.delta_seconds);
            }
            renderer.render();
        });
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
