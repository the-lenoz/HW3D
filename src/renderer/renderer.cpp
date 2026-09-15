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

Matrix4 view_translation(const Vec3& camera_position) noexcept
{
    return {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        -camera_position.x,
        -camera_position.y,
        -camera_position.z,
        1.0F,
    };
}

std::vector<float> triangle_vertices(const Configuration& configuration)
{
    std::vector<float> vertices;
    vertices.reserve(configuration.triangles.size() * 9);

    for (const Triangle& triangle : configuration.triangles) {
        for (const Vec3 vertex : {triangle.a, triangle.b, triangle.c}) {
            vertices.push_back(vertex.x);
            vertices.push_back(vertex.y);
            vertices.push_back(vertex.z);
        }
    }

    return vertices;
}

struct SceneBounds {
    Vec3 camera_position;
    float movement_step;
    float near_plane;
    float far_plane;
};

SceneBounds scene_bounds(const Configuration& configuration) noexcept
{
    Vec3 minimum{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    Vec3 maximum{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};

    for (const Triangle& triangle : configuration.triangles) {
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
    const float radius = std::max({
        (maximum.x - minimum.x) * 0.5F,
        (maximum.y - minimum.y) * 0.5F,
        (maximum.z - minimum.z) * 0.5F,
        0.5F});
    const float camera_distance =
        radius / std::tan(field_of_view_radians * 0.5F) + radius;

    return {
        .camera_position = {center.x, center.y, center.z + camera_distance},
        .movement_step = radius * 0.1F,
        .near_plane = std::max(0.01F, camera_distance - radius * 1.5F),
        .far_plane = camera_distance + radius * 2.5F,
    };
}

Renderer* renderer_from(void* const pointer) noexcept
{
    return static_cast<Renderer*>(pointer);
}

}

class Renderer::Impl final {
public:
    explicit Impl(const Configuration& configuration)
    {
        if (configuration.triangles.empty()) {
            throw std::invalid_argument(
                "renderer requires at least one triangle");
        }

        const std::vector<float> vertices = triangle_vertices(configuration);
        vertex_count_ = static_cast<GLsizei>(vertices.size() / 3);

        const SceneBounds bounds = scene_bounds(configuration);
        camera_position_ = bounds.camera_position;
        movement_step_ = bounds.movement_step;
        near_plane_ = bounds.near_plane;
        far_plane_ = bounds.far_plane;

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
            3 * sizeof(float),
            nullptr);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
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
        const Matrix4 projection = perspective(
            aspect_ratio,
            near_plane_,
            far_plane_);
        const Matrix4 view = view_translation(camera_position_);
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

    void move_forward() noexcept
    {
        camera_position_.z -= movement_step_;
    }

    void move_back() noexcept
    {
        camera_position_.z += movement_step_;
    }

    void move_left() noexcept
    {
        camera_position_.x -= movement_step_;
    }

    void move_right() noexcept
    {
        camera_position_.x += movement_step_;
    }

private:
    GLuint program_{};
    GLuint vertex_array_{};
    GLuint vertex_buffer_{};
    GLint view_projection_location_{};
    GLsizei vertex_count_{};
    Vec3 camera_position_{};
    float movement_step_{};
    float near_plane_{};
    float far_plane_{};
};

Renderer::Renderer(const Configuration& configuration)
    : impl_(std::make_unique<Impl>(configuration))
{
}

Renderer::~Renderer() = default;

void Renderer::render() noexcept
{
    impl_->render();
}

void Renderer::move_forward() noexcept
{
    impl_->move_forward();
}

void Renderer::move_back() noexcept
{
    impl_->move_back();
}

void Renderer::move_left() noexcept
{
    impl_->move_left();
}

void Renderer::move_right() noexcept
{
    impl_->move_right();
}

void render_frame(void* const renderer) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->render();
    }
}

void move_camera_forward(void* const renderer) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_forward();
    }
}

void move_camera_back(void* const renderer) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_back();
    }
}

void move_camera_left(void* const renderer) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_left();
    }
}

void move_camera_right(void* const renderer) noexcept
{
    if (Renderer* const instance = renderer_from(renderer)) {
        instance->move_right();
    }
}

}
