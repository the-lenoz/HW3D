module;

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include "embedded_shaders.hpp"

module hw3d.gpu_intersections;

import hw3d.geometry;
import hw3d.shader_program;

namespace hw3d {


namespace {

struct alignas(16) GpuTriangle {
    [[maybe_unused]] std::array<float, 4> a;
    [[maybe_unused]] std::array<float, 4> b;
    [[maybe_unused]] std::array<float, 4> c;
    [[maybe_unused]] std::array<std::int32_t, 4> voxel;
    [[maybe_unused]] std::array<float, 4> min_bound;
    [[maybe_unused]] std::array<float, 4> max_bound;
};
static_assert(sizeof(GpuTriangle) == 96);

class Buffer final {
public:
    Buffer()
    {
        glCreateBuffers(1, &handle_);
    }

    ~Buffer()
    {
        if (handle_ != 0) {
            glDeleteBuffers(1, &handle_);
        }
    }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    [[nodiscard]] GLuint handle() const noexcept
    {
        return handle_;
    }

private:
    GLuint handle_ = 0;
};

std::int32_t gpu_voxel_coordinate(const std::int64_t coordinate) noexcept
{
    return static_cast<std::int32_t>(std::clamp(
        coordinate,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
}

}

std::vector<bool> gpu_collisions(
    const std::vector<Triangle>& triangles)
{
    if (triangles.empty()) {
        return {};
    }

    std::vector<GpuTriangle> gpu_triangles;
    gpu_triangles.reserve(triangles.size());

    std::vector<Aabb> bounds;
    bounds.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        bounds.push_back(aabb(triangle));
    }
    const VoxelGrid voxel_grid = make_voxel_grid(bounds);

    for (int index = 0; index < triangles.size(); ++index) {
        const auto& [a, b, c] = triangles[index];
        const auto [x, y, z] = voxel_for(center(bounds[index]), voxel_grid);
        gpu_triangles.push_back({
            {a.x, a.y, a.z, 0.0F},
            {b.x, b.y, b.z, 0.0F},
            {c.x, c.y, c.z, 0.0F},
            {
                gpu_voxel_coordinate(x),
                gpu_voxel_coordinate(y),
                gpu_voxel_coordinate(z),
                index,
            },
            {bounds[index].minimum.x, bounds[index].minimum.y, bounds[index].minimum.z},
            {bounds[index].maximum.x, bounds[index].maximum.y, bounds[index].maximum.z}
        });
    }

    std::ranges::sort(
        gpu_triangles,
        [](const GpuTriangle &a, const GpuTriangle &b) {
            return std::tie(a.voxel[0], a.voxel[1], a.voxel[2]) < std::tie(b.voxel[0], b.voxel[1], b.voxel[2]);
        });

    std::vector<std::int32_t> gpu_results(triangles.size());

    const Buffer triangle_buffer;
    const Buffer result_buffer;

    glNamedBufferStorage(
        triangle_buffer.handle(),
        static_cast<GLsizeiptr>(
            gpu_triangles.size() * sizeof(GpuTriangle)),
        gpu_triangles.data(),
        0);
    glNamedBufferStorage(
        result_buffer.handle(),
        static_cast<GLsizeiptr>(
            gpu_results.size() * sizeof(std::int32_t)),
        nullptr,
        0);

    glClearNamedBufferData(result_buffer.handle(),
        GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);

    glBindBufferBase(
        GL_SHADER_STORAGE_BUFFER,
        0,
        triangle_buffer.handle());
    glBindBufferBase(
        GL_SHADER_STORAGE_BUFFER,
        1,
        result_buffer.handle());

    const auto program = ShaderProgram::compute(
        shaders::intersections_comp_glsl);

    program.use();

    constexpr std::size_t local_size = 64;
    const auto group_count = (triangles.size() + local_size - 1) / local_size;

    glDispatchCompute(
        static_cast<GLuint>(group_count),
        1,
        1);

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    glGetNamedBufferSubData(
        result_buffer.handle(),
        0,
        static_cast<GLsizeiptr>(
            gpu_results.size() * sizeof(std::int32_t)),
        gpu_results.data());


    std::vector<bool> result(triangles.size());
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = gpu_results[index] != 0;
    }

    return result;
}

}
