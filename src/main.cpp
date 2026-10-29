#include <cstddef>
#include <exception>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

import hw3d.configuration;
import hw3d.geometry;
import hw3d.window_context;
import hw3d.gpu_triangles;
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

void print_highlights(const std::span<const hw3d::Triangle> triangles)
{
    int space = 0;
    for (const auto &t : triangles) {
        if (space) {
            std::cout << ' ';
        }
        std::cout << (t.highlighted ? '1' : '0');
        space = 1;
    }
    std::cout << '\n';
}

void apply_camera_input(
    hw3d::Renderer& renderer,
    const hw3d::FrameInput& input) noexcept
{
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
}

}

int main(const int argc, char** argv) try
{
    const Options options = parse_options(argc, argv);
    const auto configuration = hw3d::read_configuration(std::cin);
    std::vector<hw3d::Aabb> motion_bounds(configuration.triangles.size());
    hw3d::calculate_motion_bounds(
        configuration.triangles,
        motion_bounds);
    const bool use_gpu_intersections = options.use_gpu_intersections;

    if (options.headless && !use_gpu_intersections) {
        std::vector<hw3d::Triangle> triangles(configuration.triangles.size());
        hw3d::update_vertices(
            configuration.triangles,
            triangles,
            std::chrono::duration<double>::zero());
        hw3d::CpuIntersectionDetector detector{
            triangles, motion_bounds};
        detector.update();
        print_highlights(triangles);
        return 0;
    }

    hw3d::WindowContext window{
        options.headless ? 1 : 1280,
        options.headless ? 1 : 720,
        options.headless ? "HW3D headless" : "HW3D",
        options.headless};

    if (options.headless) {
        hw3d::GpuTriangleBuffer triangles{configuration.triangles.size()};
        hw3d::GpuVertexUpdater updater{
            configuration.triangles, triangles};
        hw3d::GpuIntersectionDetector detector{
            triangles, motion_bounds};
        updater.update(std::chrono::duration<double>::zero());
        detector.update();
        std::vector<hw3d::Triangle> downloaded(configuration.triangles.size());
        triangles.download(downloaded);
        print_highlights(downloaded);
        return 0;
    }

    if (use_gpu_intersections) {
        hw3d::GpuTriangleBuffer triangles{configuration.triangles.size()};
        hw3d::GpuVertexUpdater updater{
            configuration.triangles, triangles};
        hw3d::GpuIntersectionDetector detector{
            triangles, motion_bounds};
        hw3d::Renderer renderer{triangles, motion_bounds};
        std::chrono::duration<double> elapsed{};
        const std::chrono::duration<double> simulation_duration =
            configuration.simulation_duration;

        window.run([&](const hw3d::FrameInput& input) {
            elapsed = std::min(
                elapsed + std::chrono::duration<double>{input.delta_seconds},
                simulation_duration);
            updater.update(elapsed);
            detector.update();
            apply_camera_input(renderer, input);
            renderer.render();
            if (elapsed >= simulation_duration) {
                window.request_close();
            }
        });
        return 0;
    }

    std::vector<hw3d::Triangle> triangles(configuration.triangles.size());
    hw3d::update_vertices(
        configuration.triangles,
        triangles,
        std::chrono::duration<double>::zero());
    hw3d::CpuIntersectionDetector detector{triangles, motion_bounds};
    hw3d::Renderer renderer{triangles, motion_bounds};
    std::chrono::duration<double> elapsed{};
    const std::chrono::duration<double> simulation_duration =
        configuration.simulation_duration;

    window.run([&](const hw3d::FrameInput& input) {
        elapsed = std::min(
            elapsed + std::chrono::duration<double>{input.delta_seconds},
            simulation_duration);
        hw3d::update_vertices(
            configuration.triangles,
            triangles,
            elapsed);
        detector.update();
        apply_camera_input(renderer, input);
        renderer.render();
        if (elapsed >= simulation_duration) {
            window.request_close();
        }
    });
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
