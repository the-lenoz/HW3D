module;

#include <glad/gl.h>

#include <array>
#include <cstdint>
#include <vector>

#include "embedded_shaders.hpp"

module hw3d.gpu_intersections;

import hw3d.shader_program;

namespace hw3d {


namespace {

struct alignas(16) GpuTriangle {
    std::array<float, 4> [[maybe_unused]]a;
    std::array<float, 4> [[maybe_unused]]b;
    std::array<float, 4> [[maybe_unused]]c;
};
static_assert(sizeof(GpuTriangle) == 48);

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

}

std::vector<bool> gpu_collisions(
    const std::vector<Triangle>& triangles)
{
    if (triangles.empty()) {
        return {};
    }

    std::vector<GpuTriangle> gpu_triangles;
    gpu_triangles.reserve(triangles.size());

    for (const auto&[a, b, c] : triangles) {
        gpu_triangles.push_back({
            {a.x, a.y, a.z, 0.0F},
            {b.x, b.y, b.z, 0.0F},
            {c.x, c.y, c.z, 0.0F},
        });
    }

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
