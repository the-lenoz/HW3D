module;

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

module hw3d.renderer;

namespace hw3d {
namespace {

using Matrix4 = std::array<float, 16>;

constexpr float field_of_view_radians = 1.0471975512F;
constexpr float initial_yaw_radians = -1.5707963268F;
constexpr float maximum_pitch_radians = 1.5533430343F;
constexpr float mouse_sensitivity = 0.002F;

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream input{path};
    if (!input) {
        throw std::runtime_error(
            "failed to open shader: " + path.string());
    }

    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

std::string shader_log(const GLuint shader)
{
    GLint length{};
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return log;
}

GLuint compile_shader(const GLenum type, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    const char* const source_pointer = source.c_str();
    glShaderSource(shader, 1, &source_pointer, nullptr);
    glCompileShader(shader);

    GLint compiled{};
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        const std::string log = shader_log(shader);
        glDeleteShader(shader);
        throw std::runtime_error("shader compilation failed: " + log);
    }

    return shader;
}

std::string program_log(const GLuint program)
{
    GLint length{};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    return log;
}

GLuint create_program()
{
    const std::filesystem::path shader_directory{HW3D_RENDERER_SHADER_DIR};
    const std::string vertex_source = read_text_file(
        shader_directory / "triangle.vert.glsl");
    const std::string fragment_source = read_text_file(
        shader_directory / "triangle.frag.glsl");

    const GLuint vertex_shader = compile_shader(
        GL_VERTEX_SHADER,
        vertex_source);

    GLuint fragment_shader{};
    try {
        fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    } catch (...) {
        glDeleteShader(vertex_shader);
        throw;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked{};
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        const std::string log = program_log(program);
        glDeleteProgram(program);
        throw std::runtime_error("shader program linking failed: " + log);
    }

    return program;
}

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

std::vector<float> render_vertices(
    const std::vector<Triangle>& triangles,
    const std::vector<bool>& highlighted)
{
    constexpr std::array<std::array<float, 3>, 3> barycentrics{{
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
        {0.0F, 0.0F, 1.0F},
    }};

    std::vector<float> vertices;
    vertices.reserve(triangles.size() * 3 * 7);

    for (std::size_t triangle_index = 0;
         triangle_index < triangles.size();
         ++triangle_index) {
        const Triangle& triangle = triangles[triangle_index];
        const std::array<Vec3, 3> positions{
            triangle.a,
            triangle.b,
            triangle.c,
        };

        for (std::size_t vertex_index = 0; vertex_index < 3; ++vertex_index) {
            const Vec3 vertex = positions[vertex_index];
            const auto& barycentric = barycentrics[vertex_index];
            vertices.push_back(vertex.x);
            vertices.push_back(vertex.y);
            vertices.push_back(vertex.z);
            vertices.insert(
                vertices.end(),
                barycentric.begin(),
                barycentric.end());
            vertices.push_back(highlighted[triangle_index] ? 1.0F : 0.0F);
        }
    }

    return vertices;
}

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

SceneBounds scene_bounds(const std::vector<Triangle>& triangles) noexcept
{
    Vec3 minimum{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    Vec3 maximum{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};

    for (const Triangle& triangle : triangles) {
        for (const Vec3 vertex : {triangle.a, triangle.b, triangle.c}) {
            minimum.x = std::min(minimum.x, vertex.x);
            minimum.y = std::min(minimum.y, vertex.y);
            minimum.z = std::min(minimum.z, vertex.z);
            maximum.x = std::max(maximum.x, vertex.x);
            maximum.y = std::max(maximum.y, vertex.y);
            maximum.z = std::max(maximum.z, vertex.z);
        }
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

Renderer* renderer_from(void* const pointer) noexcept
{
    return static_cast<Renderer*>(pointer);
}

}

class Renderer::Impl final {
public:
    Impl(
        const std::vector<Triangle>& triangles,
        const std::vector<bool>& highlighted)
    {
        if (triangles.empty()) {
            throw std::invalid_argument(
                "renderer requires at least one triangle");
        }
        if (triangles.size() != highlighted.size()) {
            throw std::invalid_argument(
                "triangle and highlight arrays must have equal sizes");
        }

        const std::vector<float> vertices = render_vertices(
            triangles,
            highlighted);
        vertex_count_ = static_cast<GLsizei>(vertices.size() / 7);

        const SceneBounds bounds = scene_bounds(triangles);
        camera_position_ = bounds.camera_position;
        scene_center_ = bounds.center;
        scene_radius_ = bounds.radius;
        movement_speed_ = bounds.movement_speed;

        program_ = create_program();
        view_projection_location_ =
            glGetUniformLocation(program_, "view_projection");
        if (view_projection_location_ < 0) {
            glDeleteProgram(program_);
            program_ = 0;
            throw std::runtime_error(
                "view_projection uniform was not found");
        }

        glGenVertexArrays(1, &vertex_array_);
        glGenBuffers(1, &vertex_buffer_);

        glBindVertexArray(vertex_array_);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
            vertices.data(),
            GL_STATIC_DRAW);
        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            7 * sizeof(float),
            nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            7 * sizeof(float),
            reinterpret_cast<const void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            2,
            1,
            GL_FLOAT,
            GL_FALSE,
            7 * sizeof(float),
            reinterpret_cast<const void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }

    ~Impl()
    {
        glDeleteBuffers(1, &vertex_buffer_);
        glDeleteVertexArrays(1, &vertex_array_);
        glDeleteProgram(program_);
    }

    void render() const noexcept
    {
        glClearColor(0.04F, 0.05F, 0.08F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        if (viewport[2] <= 0 || viewport[3] <= 0) {
            return;
        }

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

        glUseProgram(program_);
        glUniformMatrix4fv(
            view_projection_location_,
            1,
            GL_FALSE,
            view_projection.data());
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
    Vec3 direction() const noexcept
    {
        return camera_direction(yaw_radians_, pitch_radians_);
    }

    Vec3 right() const noexcept
    {
        constexpr Vec3 world_up{0.0F, 1.0F, 0.0F};
        return normalize(cross(direction(), world_up));
    }

    GLuint program_{};
    GLuint vertex_array_{};
    GLuint vertex_buffer_{};
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
    const std::vector<Triangle>& triangles,
    const std::vector<bool>& highlighted)
    : impl_(std::make_unique<Impl>(triangles, highlighted))
{
}

Renderer::~Renderer() = default;

void Renderer::render() noexcept
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

void render_frame(void* const renderer) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->render();
    }
}

void move_camera_forward(
    void* const renderer,
    const float delta_seconds) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_forward(delta_seconds);
    }
}

void move_camera_back(
    void* const renderer,
    const float delta_seconds) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_back(delta_seconds);
    }
}

void move_camera_left(
    void* const renderer,
    const float delta_seconds) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_left(delta_seconds);
    }
}

void move_camera_right(
    void* const renderer,
    const float delta_seconds) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_right(delta_seconds);
    }
}

void rotate_camera(
    void* const renderer,
    const float x_offset,
    const float y_offset) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->rotate(x_offset, y_offset);
    }
}

}
