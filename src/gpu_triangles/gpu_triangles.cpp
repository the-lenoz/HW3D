module;

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "embedded_shaders.hpp"

module hw3d.gpu_triangles;

import hw3d.configuration;
import hw3d.geometry;
import hw3d.shader_program;

namespace hw3d {
namespace {

constexpr std::size_t gpu_triangle_size = 80;
constexpr std::size_t compute_local_size = 64;

struct alignas(16) PackedTriangle {
    std::array<std::uint32_t, 4> a_and_highlight;
    std::array<float, 4> b;
    std::array<float, 4> c;
    std::array<float, 4> minimum;
    std::array<float, 4> maximum;
};
static_assert(sizeof(PackedTriangle) == gpu_triangle_size);

struct alignas(16) PackedInitialTriangle {
    std::array<float, 4> a;
    std::array<float, 4> b;
    std::array<float, 4> c;
    std::array<float, 4> axis_origin;
    std::array<float, 4> axis_direction_and_speed;
};
static_assert(sizeof(PackedInitialTriangle) == gpu_triangle_size);

bool finite(const Vec3& value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

std::string gl_error_message(
    const std::string_view operation,
    const GLenum error)
{
    return std::string(operation)
        + ": OpenGL error " + std::to_string(error);
}

void check_gl_error(const std::string_view operation)
{
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        throw std::runtime_error(gl_error_message(operation, error));
    }
}

GLsizeiptr storage_size(const std::size_t count)
{
    if (count > static_cast<std::size_t>(
            std::numeric_limits<GLsizeiptr>::max()) / gpu_triangle_size) {
        throw std::length_error("GPU triangle buffer is too large");
    }
    return static_cast<GLsizeiptr>(
        std::max<std::size_t>(1, count * gpu_triangle_size));
}

void validate_storage_size(const GLsizeiptr size)
{
    GLint64 maximum_storage_block_size = 0;
    glGetInteger64v(
        GL_MAX_SHADER_STORAGE_BLOCK_SIZE,
        &maximum_storage_block_size);
    check_gl_error("failed to query GPU storage-block limit");
    if (maximum_storage_block_size <= 0
        || size > maximum_storage_block_size) {
        throw std::length_error(
            "GPU triangle buffer exceeds GL_MAX_SHADER_STORAGE_BLOCK_SIZE");
    }
}

class Buffer final {
public:
    Buffer()
    {
        glCreateBuffers(1, &handle_);
        const GLenum error = glGetError();
        if (handle_ == 0 || error != GL_NO_ERROR) {
            if (handle_ != 0) {
                glDeleteBuffers(1, &handle_);
                handle_ = 0;
            }
            throw std::runtime_error("failed to create GPU triangle buffer");
        }
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

PackedTriangle pack(const Triangle& triangle)
{
    if (!finite(triangle.a)
        || !finite(triangle.b)
        || !finite(triangle.c)) {
        throw std::invalid_argument("triangle coordinates must be finite");
    }
    const Aabb bounds = aabb(triangle);
    return {
        {
            std::bit_cast<std::uint32_t>(triangle.a.x),
            std::bit_cast<std::uint32_t>(triangle.a.y),
            std::bit_cast<std::uint32_t>(triangle.a.z),
            triangle.highlighted ? 1U : 0U,
        },
        {triangle.b.x, triangle.b.y, triangle.b.z, 0.0F},
        {triangle.c.x, triangle.c.y, triangle.c.z, 0.0F},
        {
            bounds.minimum.x,
            bounds.minimum.y,
            bounds.minimum.z,
            0.0F,
        },
        {
            bounds.maximum.x,
            bounds.maximum.y,
            bounds.maximum.z,
            0.0F,
        },
    };
}

Triangle unpack(const PackedTriangle& value) noexcept
{
    return {
        .a = {
            std::bit_cast<float>(value.a_and_highlight[0]),
            std::bit_cast<float>(value.a_and_highlight[1]),
            std::bit_cast<float>(value.a_and_highlight[2]),
        },
        .b = {value.b[0], value.b[1], value.b[2]},
        .c = {value.c[0], value.c[1], value.c[2]},
        .highlighted = value.a_and_highlight[3] != 0,
    };
}

PackedInitialTriangle pack(const InitialTriangle& triangle) noexcept
{
    return {
        {triangle.a.x, triangle.a.y, triangle.a.z, 0.0F},
        {triangle.b.x, triangle.b.y, triangle.b.z, 0.0F},
        {triangle.c.x, triangle.c.y, triangle.c.z, 0.0F},
        {
            triangle.rotation_axis_origin.x,
            triangle.rotation_axis_origin.y,
            triangle.rotation_axis_origin.z,
            0.0F,
        },
        {
            triangle.rotation_axis_direction.x,
            triangle.rotation_axis_direction.y,
            triangle.rotation_axis_direction.z,
            triangle.angular_velocity_radians_per_second,
        },
    };
}

}

class GpuTriangleBuffer::Impl final {
public:
    explicit Impl(const std::size_t triangle_count)
        : triangle_count_(triangle_count)
    {
        const GLsizeiptr size = storage_size(triangle_count_);
        validate_storage_size(size);
        glNamedBufferData(buffer_.handle(), size, nullptr, GL_DYNAMIC_COPY);
        check_gl_error("failed to allocate GPU triangle buffer");
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return triangle_count_;
    }

    void bind(const std::uint32_t binding) const
    {
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<GLuint>(binding),
            buffer_.handle());
        check_gl_error("failed to bind GPU triangle buffer");
    }

    void upload(const std::span<const Triangle> triangles)
    {
        validate_size(triangles.size());
        if (triangles.empty()) {
            return;
        }
        auto* const mapped = static_cast<PackedTriangle*>(
            glMapNamedBufferRange(
                buffer_.handle(),
                0,
                static_cast<GLsizeiptr>(
                    triangle_count_ * sizeof(PackedTriangle)),
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        if (mapped == nullptr) {
            throw std::runtime_error("failed to map GPU triangle buffer");
        }
        try {
            for (std::size_t index = 0; index < triangles.size(); ++index) {
                mapped[index] = pack(triangles[index]);
            }
        } catch (...) {
            glUnmapNamedBuffer(buffer_.handle());
            throw;
        }
        if (glUnmapNamedBuffer(buffer_.handle()) != GL_TRUE) {
            throw std::runtime_error(
                "GPU triangle buffer contents became invalid");
        }
    }

    void download(const std::span<Triangle> triangles) const
    {
        validate_size(triangles.size());
        if (triangles.empty()) {
            return;
        }
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        const auto* const mapped = static_cast<const PackedTriangle*>(
            glMapNamedBufferRange(
                buffer_.handle(),
                0,
                static_cast<GLsizeiptr>(
                    triangle_count_ * sizeof(PackedTriangle)),
                GL_MAP_READ_BIT));
        if (mapped == nullptr) {
            throw std::runtime_error(
                "failed to map GPU triangle buffer for reading");
        }
        for (std::size_t index = 0; index < triangles.size(); ++index) {
            triangles[index] = unpack(mapped[index]);
        }
        if (glUnmapNamedBuffer(buffer_.handle()) != GL_TRUE) {
            throw std::runtime_error(
                "GPU triangle buffer contents became invalid");
        }
    }

private:
    void validate_size(const std::size_t size) const
    {
        if (size != triangle_count_) {
            throw std::invalid_argument(
                "GPU triangle buffer and triangle counts must match");
        }
    }

    std::size_t triangle_count_{};
    Buffer buffer_;
};

class GpuVertexUpdater::Impl final {
public:
    Impl(
        const std::span<const InitialTriangle> initial,
        GpuTriangleBuffer& current)
        : triangle_count_(initial.size()),
          current_(current)
    {
        if (current_.size() != triangle_count_) {
            throw std::invalid_argument(
                "initial and GPU triangle counts must match");
        }

        std::vector<Aabb> validation_bounds(initial.size());
        calculate_motion_bounds(initial, validation_bounds);

        std::vector<PackedInitialTriangle> packed_initial;
        packed_initial.reserve(initial.size());
        for (const InitialTriangle& triangle : initial) {
            packed_initial.push_back(pack(triangle));
        }

        const GLsizeiptr size = storage_size(triangle_count_);
        validate_storage_size(size);
        glNamedBufferData(
            initial_buffer_.handle(),
            size,
            packed_initial.empty() ? nullptr : packed_initial.data(),
            GL_STATIC_DRAW);
        check_gl_error("failed to upload initial GPU triangles");

        program_ = std::make_unique<ShaderProgram>(ShaderProgram::compute(
            shaders::update_vertices_comp_glsl));
        elapsed_location_ = program_->uniform_location("elapsed_seconds");
        if (elapsed_location_ < 0) {
            throw std::runtime_error(
                "GPU vertex updater elapsed_seconds uniform is missing");
        }

        GLint maximum_groups = 0;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maximum_groups);
        check_gl_error("failed to query GPU compute work-group limit");
        if (maximum_groups <= 0) {
            throw std::runtime_error("OpenGL reports no compute work groups");
        }
        maximum_group_count_ = static_cast<std::size_t>(maximum_groups);
    }

    void update(const std::chrono::duration<double> elapsed)
    {
        const double elapsed_seconds = elapsed.count();
        if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) {
            throw std::invalid_argument(
                "elapsed simulation time must be finite and non-negative");
        }
        if (triangle_count_ == 0) {
            return;
        }

        const std::size_t group_count =
            (triangle_count_ + compute_local_size - 1) / compute_local_size;
        if (group_count > maximum_group_count_) {
            throw std::length_error(
                "GPU vertex update dispatch exceeds driver limits");
        }

        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            initial_buffer_.handle());
        current_.bind_for_shader_storage(1);
        program_->use();
        glUniform1d(elapsed_location_, elapsed_seconds);
        glDispatchCompute(static_cast<GLuint>(group_count), 1, 1);
        check_gl_error("failed to dispatch GPU vertex update");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

private:
    std::size_t triangle_count_{};
    GpuTriangleBuffer& current_;
    Buffer initial_buffer_;
    std::unique_ptr<ShaderProgram> program_;
    GLint elapsed_location_ = -1;
    std::size_t maximum_group_count_{};
};

GpuTriangleBuffer::GpuTriangleBuffer(const std::size_t triangle_count)
    : impl_(std::make_unique<Impl>(triangle_count))
{
}

GpuTriangleBuffer::~GpuTriangleBuffer() = default;

std::size_t GpuTriangleBuffer::size() const noexcept
{
    return impl_->size();
}

void GpuTriangleBuffer::bind_for_shader_storage(
    const std::uint32_t binding) const
{
    impl_->bind(binding);
}

void GpuTriangleBuffer::upload(const std::span<const Triangle> triangles)
{
    impl_->upload(triangles);
}

void GpuTriangleBuffer::download(const std::span<Triangle> triangles) const
{
    impl_->download(triangles);
}

GpuVertexUpdater::GpuVertexUpdater(
    const std::span<const InitialTriangle> initial,
    GpuTriangleBuffer& current)
    : impl_(std::make_unique<Impl>(initial, current))
{
}

GpuVertexUpdater::~GpuVertexUpdater() = default;

void GpuVertexUpdater::update(const std::chrono::duration<double> elapsed)
{
    impl_->update(elapsed);
}

}
