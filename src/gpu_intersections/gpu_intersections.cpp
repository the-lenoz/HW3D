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
#include <string>
#include <string_view>
#include <vector>

#include "embedded_shaders.hpp"

module hw3d.gpu_intersections;

import hw3d.geometry;
import hw3d.gpu_triangles;
import hw3d.shader_program;

namespace hw3d {
namespace {

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
        const GLenum error = glGetError();
        if (handle_ == 0 || error != GL_NO_ERROR) {
            if (handle_ != 0) {
                glDeleteBuffers(1, &handle_);
                handle_ = 0;
            }
            throw std::runtime_error(
                "failed to create GPU intersection buffer");
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

std::int32_t gpu_voxel_coordinate(const std::int64_t coordinate) noexcept
{
    return static_cast<std::int32_t>(std::clamp(
        coordinate,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) + 1,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) - 1));
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

std::string gl_error_message(
    const std::string_view operation,
    const GLenum error)
{
    std::string reason;
    if (error == GL_OUT_OF_MEMORY) {
        reason = "out of GPU memory";
    } else if (error == GL_INVALID_VALUE) {
        reason = "invalid value";
    } else if (error == GL_INVALID_OPERATION) {
        reason = "invalid operation";
    } else {
        reason = "OpenGL error " + std::to_string(error);
    }
    return std::string(operation) + ": " + reason;
}

void check_gl_error(const std::string_view operation)
{
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        throw std::runtime_error(gl_error_message(operation, error));
    }
}

void allocate_required_storage(
    const Buffer& buffer,
    const GLsizeiptr size,
    const void* const data,
    const GLenum usage,
    const GLint64 maximum_storage_block_size,
    const std::string_view label)
{
    if (maximum_storage_block_size <= 0
        || size > maximum_storage_block_size) {
        throw std::length_error(
            std::string(label)
            + " exceeds GL_MAX_SHADER_STORAGE_BLOCK_SIZE");
    }
    glNamedBufferData(buffer.handle(), size, data, usage);
    check_gl_error(std::string("failed to allocate ") + std::string(label));
}

}

class GpuIntersectionDetector::Impl final {
public:
    Impl(
        GpuTriangleBuffer& triangles,
        const std::span<const Aabb> motion_bounds,
        const std::size_t maximum_pair_count)
        : triangles_(triangles),
          triangle_count_(triangles.size())
    {
        if (motion_bounds.size() != triangle_count_) {
            throw std::invalid_argument(
                "triangle and motion bound counts must match");
        }
        if (maximum_pair_count > maximum_cached_triangle_pair_count) {
            throw std::invalid_argument(
                "intersection pair budget exceeds the supported maximum");
        }
        if (triangle_count_ >= static_cast<std::size_t>(
                std::numeric_limits<std::int32_t>::max())) {
            throw std::length_error(
                "too many triangles for GPU intersection indices");
        }

        GLint64 maximum_storage_block_size = 0;
        glGetInteger64v(
            GL_MAX_SHADER_STORAGE_BLOCK_SIZE,
            &maximum_storage_block_size);
        check_gl_error("failed to query GPU storage-block limit");
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

        if (candidates.pairs.has_value()
            && allocate_pair_buffers(
                *candidates.pairs,
                maximum_pairs_per_buffer)) {
            pair_mode_ = true;
            pair_count_ = candidates.pairs->size();
            allocate_required_storage(
                static_entry_buffer_,
                1,
                nullptr,
                GL_STATIC_DRAW,
                maximum_storage_block_size,
                "GPU intersection static-entry placeholder");
        } else {
            build_fallback_entries(
                candidates.voxels,
                maximum_storage_block_size);
            allocate_required_storage(
                fallback_pair_buffer_,
                1,
                nullptr,
                GL_STATIC_DRAW,
                maximum_storage_block_size,
                "GPU intersection pair placeholder");
        }

        program_.emplace(ShaderProgram::compute(
            shaders::intersections_comp_glsl));
        operation_mode_location_ = program_->uniform_location("operation_mode");
        pair_offset_location_ = program_->uniform_location("pair_offset");
        if (operation_mode_location_ < 0 || pair_offset_location_ < 0) {
            throw std::runtime_error(
                "intersection compute shader uniforms are missing");
        }

        GLint maximum_groups = 0;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maximum_groups);
        check_gl_error("failed to query GPU compute work-group limit");
        if (maximum_groups <= 0) {
            throw std::runtime_error(
                "OpenGL reports no compute work groups");
        }
        maximum_group_count_ = static_cast<std::size_t>(maximum_groups);
    }

    void update()
    {
        if (triangle_count_ == 0) {
            return;
        }

        triangles_.bind_for_shader_storage(0);
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            3,
            static_entry_buffer_.handle());
        program_->use();

        glUniform1i(operation_mode_location_, 0);
        glUniform1ui(pair_offset_location_, 0);
        dispatch(triangle_count_);
        check_gl_error("failed to clear GPU intersection highlights");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        const bool has_work = !pair_mode_ || pair_count_ != 0;
        if (!has_work) {
            return;
        }

        if (pair_mode_) {
            glUniform1i(operation_mode_location_, 1);
            dispatch_pairs();
        } else {
            glBindBufferBase(
                GL_SHADER_STORAGE_BUFFER,
                2,
                fallback_pair_buffer_.handle());
            glUniform1i(operation_mode_location_, 2);
            glUniform1ui(pair_offset_location_, 0);
            dispatch(triangle_count_);
        }

        check_gl_error("failed to dispatch GPU intersections");
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
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

    void build_fallback_entries(
        const std::span<const Voxel> voxels,
        const GLint64 maximum_storage_block_size)
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
        allocate_required_storage(
            static_entry_buffer_,
            storage_size(static_entries.size(), sizeof(GpuStaticEntry)),
            static_entries.empty() ? nullptr : static_entries.data(),
            GL_STATIC_DRAW,
            maximum_storage_block_size,
            "GPU intersection static-entry buffer");
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

    GpuTriangleBuffer& triangles_;
    std::size_t triangle_count_{};
    bool pair_mode_ = false;
    std::size_t pair_count_{};
    std::size_t maximum_group_count_{};
    GLint operation_mode_location_ = -1;
    GLint pair_offset_location_ = -1;
    std::vector<PairBufferChunk> pair_buffers_;
    Buffer fallback_pair_buffer_;
    Buffer static_entry_buffer_;
    std::optional<ShaderProgram> program_;
};

GpuIntersectionDetector::GpuIntersectionDetector(
    GpuTriangleBuffer& triangles,
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
