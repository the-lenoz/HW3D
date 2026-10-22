module;

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "embedded_shaders.hpp"

module hw3d.gpu_intersections;

import hw3d.configuration;
import hw3d.geometry;
import hw3d.shader_program;

namespace hw3d {
namespace {

struct alignas(16) GpuTriangle {
    [[maybe_unused]] std::array<float, 4> a;
    [[maybe_unused]] std::array<float, 4> b;
    [[maybe_unused]] std::array<float, 4> c;
    [[maybe_unused]] std::array<float, 4> min_bound;
    [[maybe_unused]] std::array<float, 4> max_bound;
};
static_assert(sizeof(GpuTriangle) == 80);
static_assert(sizeof(TrianglePair) == 8);

struct alignas(16) GpuStaticEntry {
    std::array<std::int32_t, 4> voxel;
};
static_assert(sizeof(GpuStaticEntry) == 16);

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

bool finite(const Vec3& point) noexcept
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z);
}

GLsizeiptr storage_size(
    const std::size_t count,
    const std::size_t element_size)
{
    if (count > static_cast<std::size_t>(
            std::numeric_limits<GLsizeiptr>::max()) / element_size) {
        throw std::length_error("OpenGL intersection buffer is too large");
    }
    return static_cast<GLsizeiptr>(
        std::max<std::size_t>(1, count) * element_size);
}

}

class GpuIntersectionDetector::Impl final {
public:
    Impl(
        std::vector<Triangle>& triangles,
        const std::span<const Aabb> motion_bounds,
        const std::size_t maximum_pair_count)
        : triangles_(triangles),
          triangle_count_(triangles.size()),
          gpu_triangles_(triangle_count_),
          gpu_results_(triangle_count_)
    {
        if (motion_bounds.size() != triangle_count_) {
            throw std::invalid_argument(
                "triangle and motion bound counts must match");
        }
        if (maximum_pair_count > maximum_cached_triangle_pair_count) {
            throw std::invalid_argument(
                "intersection pair budget exceeds the supported maximum");
        }

        GLint64 maximum_storage_block_size = 0;
        glGetInteger64v(
            GL_MAX_SHADER_STORAGE_BLOCK_SIZE,
            &maximum_storage_block_size);
        const std::size_t maximum_pairs_per_buffer =
            maximum_storage_block_size > 0
            ? std::min(
                static_cast<std::size_t>(maximum_storage_block_size)
                    / sizeof(TrianglePair),
                static_cast<std::size_t>(
                    std::numeric_limits<GLuint>::max()))
            : 0;
        MotionPairCandidates candidates = make_motion_pair_candidates(
            motion_bounds,
            maximum_pair_count);

        glNamedBufferData(
            triangle_buffer_.handle(),
            storage_size(triangle_count_, sizeof(GpuTriangle)),
            nullptr,
            GL_STREAM_DRAW);
        glNamedBufferData(
            result_buffer_.handle(),
            storage_size(triangle_count_, sizeof(std::int32_t)),
            nullptr,
            GL_STREAM_READ);

        if (candidates.pairs.has_value()
            && allocate_pair_buffers(
                *candidates.pairs,
                maximum_pairs_per_buffer)) {
            pair_mode_ = true;
            pair_count_ = candidates.pairs->size();
            glNamedBufferData(
                static_entry_buffer_.handle(),
                1,
                nullptr,
                GL_STATIC_DRAW);
        } else {
            build_fallback_entries(candidates.voxels);
            glNamedBufferData(
                fallback_pair_buffer_.handle(),
                1,
                nullptr,
                GL_STATIC_DRAW);
        }

        program_.emplace(ShaderProgram::compute(
            shaders::intersections_comp_glsl));
        pair_mode_location_ = program_->uniform_location("pair_mode");
        pair_offset_location_ = program_->uniform_location("pair_offset");
        if (pair_mode_location_ < 0 || pair_offset_location_ < 0) {
            throw std::runtime_error(
                "intersection compute shader uniforms are missing");
        }

        GLint maximum_groups = 0;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maximum_groups);
        if (maximum_groups <= 0) {
            throw std::runtime_error(
                "OpenGL reports no compute work groups");
        }
        maximum_group_count_ = static_cast<std::size_t>(maximum_groups);
    }

    void update()
    {
        if (triangles_.size() != triangle_count_) {
            throw std::logic_error(
                "intersection detector triangle count cannot change");
        }

        const bool has_work = triangle_count_ != 0
            && (!pair_mode_ || pair_count_ != 0);
        for (std::size_t id = 0; id < triangle_count_; ++id) {
            Triangle& triangle = triangles_[id];
            if (!finite(triangle.a)
                || !finite(triangle.b)
                || !finite(triangle.c)) {
                throw std::invalid_argument(
                    "triangle coordinates must be finite");
            }
            triangle.highlighted = false;
            if (!has_work) {
                continue;
            }
            const Aabb bounds = aabb(triangle);
            gpu_triangles_[id] = {
                {triangle.a.x, triangle.a.y, triangle.a.z, 0.0F},
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
        if (!has_work) {
            return;
        }

        glNamedBufferSubData(
            triangle_buffer_.handle(),
            0,
            static_cast<GLsizeiptr>(
                triangle_count_ * sizeof(GpuTriangle)),
            gpu_triangles_.data());
        glClearNamedBufferData(
            result_buffer_.handle(),
            GL_R32UI,
            GL_RED_INTEGER,
            GL_UNSIGNED_INT,
            nullptr);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            triangle_buffer_.handle());
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            1,
            result_buffer_.handle());
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            3,
            static_entry_buffer_.handle());
        program_->use();

        if (pair_mode_) {
            glUniform1i(pair_mode_location_, 1);
            dispatch_pairs();
        } else {
            glBindBufferBase(
                GL_SHADER_STORAGE_BUFFER,
                2,
                fallback_pair_buffer_.handle());
            glUniform1i(pair_mode_location_, 0);
            glUniform1ui(pair_offset_location_, 0);
            dispatch(triangle_count_);
        }

        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        glGetNamedBufferSubData(
            result_buffer_.handle(),
            0,
            static_cast<GLsizeiptr>(
                triangle_count_ * sizeof(std::int32_t)),
            gpu_results_.data());

        for (std::size_t index = 0; index < triangle_count_; ++index) {
            triangles_[index].highlighted = gpu_results_[index] != 0;
        }
    }

private:
    struct PairBufferChunk {
        std::unique_ptr<Buffer> buffer;
        std::size_t pair_count;
    };

    bool allocate_pair_buffers(
        const std::vector<TrianglePair>& pairs,
        const std::size_t maximum_pairs_per_buffer)
    {
        if (pairs.empty()) {
            return true;
        }
        if (maximum_pairs_per_buffer == 0) {
            return false;
        }

        pair_buffers_.reserve(
            (pairs.size() + maximum_pairs_per_buffer - 1)
            / maximum_pairs_per_buffer);
        for (std::size_t offset = 0; offset < pairs.size();) {
            const std::size_t count = std::min(
                pairs.size() - offset,
                maximum_pairs_per_buffer);
            auto buffer = std::make_unique<Buffer>();
            glNamedBufferData(
                buffer->handle(),
                storage_size(count, sizeof(TrianglePair)),
                pairs.data() + offset,
                GL_STATIC_DRAW);
            const GLenum error = glGetError();
            if (error == GL_OUT_OF_MEMORY) {
                pair_buffers_.clear();
                return false;
            }
            if (error != GL_NO_ERROR) {
                throw std::runtime_error(
                    "failed to create GPU intersection pair buffer");
            }
            pair_buffers_.push_back({std::move(buffer), count});
            offset += count;
        }
        return true;
    }

    void build_fallback_entries(const std::span<const Voxel> voxels)
    {
        std::vector<GpuStaticEntry> static_entries;
        static_entries.reserve(voxels.size());
        for (std::size_t id = 0; id < voxels.size(); ++id) {
            static_entries.push_back({{
                gpu_voxel_coordinate(voxels[id].x),
                gpu_voxel_coordinate(voxels[id].y),
                gpu_voxel_coordinate(voxels[id].z),
                static_cast<std::int32_t>(id),
            }});
        }
        std::ranges::sort(
            static_entries,
            [](const GpuStaticEntry& left, const GpuStaticEntry& right) {
                return std::array{
                    left.voxel[0], left.voxel[1], left.voxel[2],
                } < std::array{
                    right.voxel[0], right.voxel[1], right.voxel[2],
                };
            });
        glNamedBufferData(
            static_entry_buffer_.handle(),
            storage_size(static_entries.size(), sizeof(GpuStaticEntry)),
            static_entries.empty() ? nullptr : static_entries.data(),
            GL_STATIC_DRAW);
    }

    void dispatch(const std::size_t invocation_count) const
    {
        constexpr std::size_t local_size = 64;
        const std::size_t group_count =
            (invocation_count + local_size - 1) / local_size;
        if (group_count > maximum_group_count_) {
            throw std::length_error(
                "GPU intersection dispatch exceeds driver limits");
        }
        glDispatchCompute(static_cast<GLuint>(group_count), 1, 1);
    }

    void dispatch_pairs() const
    {
        constexpr std::size_t local_size = 64;
        const std::size_t maximum_batch =
            maximum_group_count_ * local_size;
        for (const PairBufferChunk& chunk : pair_buffers_) {
            glBindBufferBase(
                GL_SHADER_STORAGE_BUFFER,
                2,
                chunk.buffer->handle());
            for (std::size_t offset = 0; offset < chunk.pair_count;) {
                const std::size_t batch = std::min(
                    chunk.pair_count - offset,
                    maximum_batch);
                glUniform1ui(
                    pair_offset_location_,
                    static_cast<GLuint>(offset));
                dispatch(batch);
                offset += batch;
            }
        }
    }

    std::vector<Triangle>& triangles_;
    std::size_t triangle_count_{};
    bool pair_mode_ = false;
    std::size_t pair_count_{};
    std::size_t maximum_group_count_{};
    GLint pair_mode_location_ = -1;
    GLint pair_offset_location_ = -1;
    std::vector<GpuTriangle> gpu_triangles_;
    std::vector<std::int32_t> gpu_results_;
    std::vector<PairBufferChunk> pair_buffers_;
    Buffer triangle_buffer_;
    Buffer result_buffer_;
    Buffer fallback_pair_buffer_;
    Buffer static_entry_buffer_;
    std::optional<ShaderProgram> program_;
};

GpuIntersectionDetector::GpuIntersectionDetector(
    std::vector<Triangle>& triangles,
    const std::span<const Aabb> motion_bounds,
    const std::size_t maximum_pair_count)
    : impl_(std::make_unique<Impl>(
          triangles,
          motion_bounds,
          maximum_pair_count))
{
}

GpuIntersectionDetector::~GpuIntersectionDetector() = default;

void GpuIntersectionDetector::update()
{
    impl_->update();
}

}
