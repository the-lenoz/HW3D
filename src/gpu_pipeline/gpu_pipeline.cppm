module;

#include <chrono>
#include <cstddef>
#include <memory>

export module hw3d.gpu_pipeline;

import hw3d.configuration;
import hw3d.geometry;
import hw3d.window_context;

export namespace hw3d {

struct GpuPipelineOptions {
    std::size_t maximum_cached_pair_count =
        maximum_cached_triangle_pair_count;
};

class GpuPipeline final {
public:
    GpuPipeline(
        WindowContext& window,
        const Configuration& configuration,
        GpuPipelineOptions options = {});
    ~GpuPipeline();

    GpuPipeline(const GpuPipeline&) = delete;
    GpuPipeline& operator=(const GpuPipeline&) = delete;
    GpuPipeline(GpuPipeline&&) = delete;
    GpuPipeline& operator=(GpuPipeline&&) = delete;

    [[nodiscard]] bool render_frame(
        std::chrono::duration<float> elapsed,
        const FrameInput& input);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
