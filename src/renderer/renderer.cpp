module;

#include <glad/gl.h>
#include "embedded_shaders.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

module hw3d.renderer;
import hw3d.configuration;
import hw3d.geometry;
import hw3d.shader_program;

namespace hw3d {
namespace {

using Matrix4 = std::array<float, 16>;

constexpr float field_of_view_radians = 1.0471975512F;
constexpr float initial_yaw_radians = -1.5707963268F;
constexpr float maximum_pitch_radians = 1.5533430343F;
constexpr float mouse_sensitivity = 0.002F;

Matrix4 multiply(const Matrix4& left, const Matrix4& right) noexcept
{
    Matrix4 result{};

    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t index = 0; index < 4; ++index) {
                result[column * 4 + row] +=
                    left[index * 4 + row]
                    * right[column * 4 + index];
            }
        }
    }

    return result;
}

Matrix4 perspective(
    const float aspect_ratio,
    const float near_plane,
    const float far_plane) noexcept
{
    const float focal_length =
        1.0F / std::tan(field_of_view_radians * 0.5F);

    return {
        focal_length / aspect_ratio, 0.0F, 0.0F, 0.0F,
        0.0F, focal_length, 0.0F, 0.0F,
        0.0F, 0.0F,
        (far_plane + near_plane) / (near_plane - far_plane), -1.0F,
        0.0F, 0.0F,
        (2.0F * far_plane * near_plane) / (near_plane - far_plane), 0.0F,
    };
}

Vec3 add(const Vec3& left, const Vec3& right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 scale(const Vec3& vector, const float factor) noexcept
{
    return {vector.x * factor, vector.y * factor, vector.z * factor};
}

float dot(const Vec3& left, const Vec3& right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(const Vec3& left, const Vec3& right) noexcept
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float length(const Vec3& vector) noexcept
{
    return std::sqrt(dot(vector, vector));
}

Vec3 normalize(const Vec3& vector) noexcept
{
    return scale(vector, 1.0F / length(vector));
}

Vec3 camera_direction(
    const float yaw_radians,
    const float pitch_radians) noexcept
{
    const float pitch_cosine = std::cos(pitch_radians);
    return normalize({
        std::cos(yaw_radians) * pitch_cosine,
        std::sin(pitch_radians),
        std::sin(yaw_radians) * pitch_cosine,
    });
}

Matrix4 look_at(
    const Vec3& camera_position,
    const Vec3& direction) noexcept
{
    constexpr Vec3 world_up{0.0F, 1.0F, 0.0F};
    const Vec3 forward = normalize(direction);
    const Vec3 right = normalize(cross(forward, world_up));
    const Vec3 up = cross(right, forward);

    return {
        right.x, up.x, -forward.x, 0.0F,
        right.y, up.y, -forward.y, 0.0F,
        right.z, up.z, -forward.z, 0.0F,
        -dot(right, camera_position),
        -dot(up, camera_position),
        dot(forward, camera_position),
        1.0F,
    };
}

Vec3 triangle_normal(const Triangle& triangle) noexcept
{
    const Vec3 first_edge{
        triangle.b.x - triangle.a.x,
        triangle.b.y - triangle.a.y,
        triangle.b.z - triangle.a.z};
    const Vec3 second_edge{
        triangle.c.x - triangle.a.x,
        triangle.c.y - triangle.a.y,
        triangle.c.z - triangle.a.z};
    const Vec3 normal = cross(first_edge, second_edge);
    const float normal_length = length(normal);

    if (!std::isfinite(normal_length)
        || normal_length <= std::numeric_limits<float>::epsilon()) {
        return {0.0F, 1.0F, 0.0F};
    }

    return scale(normal, 1.0F / normal_length);
}

struct alignas(16) GpuTriangle {
    std::array<float, 4> a;
    std::array<float, 4> b;
    std::array<float, 4> c;
    std::array<float, 4> normal;
};
static_assert(sizeof(GpuTriangle) == 64);

struct SceneBounds {
    Vec3 camera_position;
    Vec3 center;
    float radius;
    float movement_speed;
};

struct ClipPlanes {
    float near_plane;
    float far_plane;
};

bool valid_motion_bounds(const Aabb& bounds) noexcept
{
    const auto finite = [](const Vec3& point) noexcept {
        return std::isfinite(point.x)
            && std::isfinite(point.y)
            && std::isfinite(point.z);
    };
    return finite(bounds.minimum)
        && finite(bounds.maximum)
        && bounds.minimum.x <= bounds.maximum.x
        && bounds.minimum.y <= bounds.maximum.y
        && bounds.minimum.z <= bounds.maximum.z;
}

SceneBounds scene_bounds(const std::span<const Aabb> motion_bounds) noexcept
{
    Vec3 minimum{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    Vec3 maximum{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};

    for (const Aabb& bounds : motion_bounds) {
        minimum.x = std::min(minimum.x, bounds.minimum.x);
        minimum.y = std::min(minimum.y, bounds.minimum.y);
        minimum.z = std::min(minimum.z, bounds.minimum.z);
        maximum.x = std::max(maximum.x, bounds.maximum.x);
        maximum.y = std::max(maximum.y, bounds.maximum.y);
        maximum.z = std::max(maximum.z, bounds.maximum.z);
    }

    const Vec3 center{
        (minimum.x + maximum.x) * 0.5F,
        (minimum.y + maximum.y) * 0.5F,
        (minimum.z + maximum.z) * 0.5F};
    const Vec3 half_extent{
        (maximum.x - minimum.x) * 0.5F,
        (maximum.y - minimum.y) * 0.5F,
        (maximum.z - minimum.z) * 0.5F};
    const float radius = std::max(length(half_extent), 0.5F);
    const float camera_distance =
        radius / std::tan(field_of_view_radians * 0.5F) + radius;

    return {
        .camera_position = {center.x, center.y, center.z + camera_distance},
        .center = center,
        .radius = radius,
        .movement_speed = radius * 1.5F,
    };
}

ClipPlanes clip_planes(
    const Vec3& camera_position,
    const Vec3& scene_center,
    const float scene_radius) noexcept
{
    constexpr float clipping_margin_factor = 1.1F;
    constexpr float minimum_near_factor = 0.001F;
    constexpr float absolute_minimum_near = 0.0001F;

    const Vec3 camera_to_center{
        scene_center.x - camera_position.x,
        scene_center.y - camera_position.y,
        scene_center.z - camera_position.z};
    const float distance_to_center = length(camera_to_center);
    const float padded_radius = scene_radius * clipping_margin_factor;
    const float minimum_near = std::max(
        scene_radius * minimum_near_factor,
        absolute_minimum_near);
    const float near_plane = std::max(
        minimum_near,
        distance_to_center - padded_radius);
    const float far_plane = std::max(
        distance_to_center + padded_radius,
        std::nextafter(near_plane, std::numeric_limits<float>::max()));

    return {
        .near_plane = near_plane,
        .far_plane = far_plane,
    };
}

}

class Renderer::Impl final {
public:
    Impl(
        const std::span<const Triangle> triangles,
        const std::span<const Aabb> motion_bounds)
        : triangles_(triangles)
    {
        if (triangles.empty()) {
            throw std::invalid_argument(
                "renderer requires at least one triangle");
        }
        if (motion_bounds.size() != triangles_.size()) {
            throw std::invalid_argument(
                "triangle and motion bound counts must match");
        }
        if (!std::ranges::all_of(
                motion_bounds, valid_motion_bounds)) {
            throw std::invalid_argument(
                "motion bounds must be finite and ordered");
        }
        vertex_count_ = static_cast<GLsizei>(triangles_.size() * 3);

        const SceneBounds bounds = scene_bounds(motion_bounds);
        camera_position_ = bounds.camera_position;
        scene_center_ = bounds.center;
        scene_radius_ = bounds.radius;
        movement_speed_ = bounds.movement_speed;

        program_.emplace(ShaderProgram::graphics(
            shaders::triangle_vert_glsl,
            shaders::triangle_frag_glsl));
        view_projection_location_ = program_->uniform_location("view_projection");
        if (view_projection_location_ < 0) {
            throw std::runtime_error(
                "view_projection uniform was not found");
        }

        glGenVertexArrays(1, &vertex_array_);
        glCreateBuffers(1, &triangle_buffer_);
        glNamedBufferData(
            triangle_buffer_,
            static_cast<GLsizeiptr>(triangles_.size() * sizeof(GpuTriangle)),
            nullptr,
            GL_STREAM_DRAW);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }

    ~Impl()
    {
        glDeleteBuffers(1, &triangle_buffer_);
        glDeleteVertexArrays(1, &vertex_array_);
    }

    void render()
    {
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        if (viewport[2] <= 0 || viewport[3] <= 0) {
            return;
        }

        update_triangle_buffer();

        glClearColor(0.04F, 0.05F, 0.08F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float aspect_ratio =
            static_cast<float>(viewport[2])
            / static_cast<float>(viewport[3]);
        const ClipPlanes clipping = clip_planes(
            camera_position_,
            scene_center_,
            scene_radius_);
        const Matrix4 projection = perspective(
            aspect_ratio,
            clipping.near_plane,
            clipping.far_plane);
        const Matrix4 view = look_at(camera_position_, direction());
        const Matrix4 view_projection = multiply(projection, view);

        program_->use();
        glUniformMatrix4fv(
            view_projection_location_,
            1,
            GL_FALSE,
            view_projection.data());
        glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            triangle_buffer_);
        glBindVertexArray(vertex_array_);
        glDrawArrays(GL_TRIANGLES, 0, vertex_count_);
        glBindVertexArray(0);
    }

    void move_forward(const float delta_seconds) noexcept
    {
        camera_position_ = add(
            camera_position_,
            scale(direction(), movement_speed_ * delta_seconds));
    }

    void move_back(const float delta_seconds) noexcept
    {
        camera_position_ = add(
            camera_position_,
            scale(direction(), -movement_speed_ * delta_seconds));
    }

    void move_left(const float delta_seconds) noexcept
    {
        camera_position_ = add(
            camera_position_,
            scale(right(), -movement_speed_ * delta_seconds));
    }

    void move_right(const float delta_seconds) noexcept
    {
        camera_position_ = add(
            camera_position_,
            scale(right(), movement_speed_ * delta_seconds));
    }

    void rotate(const float x_offset, const float y_offset) noexcept
    {
        yaw_radians_ += x_offset * mouse_sensitivity;
        pitch_radians_ = std::clamp(
            pitch_radians_ + y_offset * mouse_sensitivity,
            -maximum_pitch_radians,
            maximum_pitch_radians);
    }

private:
    void update_triangle_buffer()
    {
        auto* const mapped = static_cast<GpuTriangle*>(
            glMapNamedBufferRange(
                triangle_buffer_,
                0,
                static_cast<GLsizeiptr>(
                    triangles_.size() * sizeof(GpuTriangle)),
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        if (mapped == nullptr) {
            throw std::runtime_error(
                "failed to map renderer triangle buffer");
        }

        for (std::size_t index = 0; index < triangles_.size(); ++index) {
            const Triangle& triangle = triangles_[index];
            const Vec3 normal = triangle_normal(triangle);
            mapped[index] = {
                {
                    triangle.a.x,
                    triangle.a.y,
                    triangle.a.z,
                    triangle.highlighted ? 1.0F : 0.0F,
                },
                {triangle.b.x, triangle.b.y, triangle.b.z, 0.0F},
                {triangle.c.x, triangle.c.y, triangle.c.z, 0.0F},
                {normal.x, normal.y, normal.z, 0.0F},
            };
        }

        if (glUnmapNamedBuffer(triangle_buffer_) != GL_TRUE) {
            throw std::runtime_error(
                "renderer triangle buffer contents became invalid");
        }
    }

    Vec3 direction() const noexcept
    {
        return camera_direction(yaw_radians_, pitch_radians_);
    }

    Vec3 right() const noexcept
    {
        constexpr Vec3 world_up{0.0F, 1.0F, 0.0F};
        return normalize(cross(direction(), world_up));
    }

    std::optional<ShaderProgram> program_;
    std::span<const Triangle> triangles_;
    GLuint vertex_array_{};
    GLuint triangle_buffer_{};
    GLint view_projection_location_{};
    GLsizei vertex_count_{};
    Vec3 camera_position_{};
    Vec3 scene_center_{};
    float scene_radius_{};
    float movement_speed_{};
    float yaw_radians_ = initial_yaw_radians;
    float pitch_radians_{};
};

Renderer::Renderer(
    const std::span<const Triangle> triangles,
    const std::span<const Aabb> motion_bounds)
    : impl_(std::make_unique<Impl>(triangles, motion_bounds))
{
}

Renderer::~Renderer() = default;

void Renderer::render()
{
    impl_->render();
}

void Renderer::move_forward(const float delta_seconds) noexcept
{
    impl_->move_forward(delta_seconds);
}

void Renderer::move_back(const float delta_seconds) noexcept
{
    impl_->move_back(delta_seconds);
}

void Renderer::move_left(const float delta_seconds) noexcept
{
    impl_->move_left(delta_seconds);
}

void Renderer::move_right(const float delta_seconds) noexcept
{
    impl_->move_right(delta_seconds);
}

void Renderer::rotate(
    const float x_offset,
    const float y_offset) noexcept
{
    impl_->rotate(x_offset, y_offset);
}

}
