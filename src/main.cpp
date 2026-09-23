#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

import hw3d.configuration;
import hw3d.window_context;
import hw3d.renderer;
import hw3d.cpu_intersections;
import hw3d.gpu_intersections;

namespace {

struct Options {
    bool headless = false;
    bool use_gpu_intersections = false;
};

Options parse_options(const int argc, char** const argv)
{
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--headless") {
            if (options.headless) {
                throw std::invalid_argument(
                    "duplicate command-line option: --headless");
            }
            options.headless = true;
        } else if (argument == "--use-gpu-intersections") {
            if (options.use_gpu_intersections) {
                throw std::invalid_argument(
                    "duplicate command-line option: "
                    "--use-gpu-intersections");
            }
            options.use_gpu_intersections = true;
        } else {
            throw std::invalid_argument(
                "unknown command-line option: " + std::string(argument));
        }
    }

    return options;
}

void print_collisions(const std::vector<bool>& collisions)
{
    for (std::size_t index = 0; index < collisions.size(); ++index) {
        if (index != 0) {
            std::cout << ' ';
        }
        std::cout << (collisions[index] ? '1' : '0');
    }
    std::cout << '\n';
}

}

int main(const int argc, char** argv)
{
    try {
        const Options options = parse_options(argc, argv);
        const auto configuration = hw3d::read_configuration(std::cin);

        if (options.headless && !options.use_gpu_intersections) {
            print_collisions(
                hw3d::cpu_collisions(configuration.triangles));
            return 0;
        }

        hw3d::WindowContext window{
            options.headless ? 1 : 1280,
            options.headless ? 1 : 720,
            options.headless ? "HW3D headless" : "HW3D",
            options.headless};

        const std::vector<bool> highlighted = options.use_gpu_intersections
            ? hw3d::gpu_collisions(configuration.triangles)
            : hw3d::cpu_collisions(configuration.triangles);

        if (options.headless) {
            print_collisions(highlighted);
            return 0;
        }

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
