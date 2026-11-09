#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

import hw3d.configuration;
import hw3d.gpu_pipeline;
import hw3d.window_context;

namespace {

void validate_arguments(const int argc, char** const argv)
{
    if (argc > 1) {
        throw std::invalid_argument(
            "unknown command-line option: " + std::string(argv[1]));
    }
}

}

int main(const int argc, char** argv) try
{
    validate_arguments(argc, argv);
    const hw3d::Configuration configuration =
        hw3d::read_configuration(std::cin);

    hw3d::WindowContext window{1280, 720, "HW3D"};
    hw3d::GpuPipeline pipeline{window, configuration};
    std::chrono::duration<double> elapsed{};
    const std::chrono::duration<double> duration =
        configuration.simulation_duration;
    window.run([&](const hw3d::FrameInput& input) {
        elapsed = std::min(
            elapsed + std::chrono::duration<double>{input.delta_seconds},
            duration);
        const bool presented = pipeline.render_frame(
            std::chrono::duration<float>{
                static_cast<float>(elapsed.count())},
            input);
        if (presented && elapsed >= duration) {
            window.request_close();
        }
    });
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
