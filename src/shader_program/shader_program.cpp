module;

#include <glad/gl.h>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

module hw3d.shader_program;

namespace hw3d {
namespace {

struct ShaderObject {
    GLuint handle;

    explicit ShaderObject(GLuint value) noexcept : handle(value) {}
    ShaderObject(const ShaderObject&) = delete;
    ShaderObject& operator=(const ShaderObject&) = delete;
    ~ShaderObject()
    {
        if (handle != 0) {
            glDeleteShader(handle);
        }
    }
    [[nodiscard]] GLuint release() noexcept { return std::exchange(handle, 0); }
};

struct ProgramObject {
    GLuint handle;

    explicit ProgramObject(GLuint value) noexcept : handle(value) {}
    ProgramObject(const ProgramObject&) = delete;
    ProgramObject& operator=(const ProgramObject&) = delete;
    ~ProgramObject()
    {
        if (handle != 0) {
            glDeleteProgram(handle);
        }
    }
    [[nodiscard]] GLuint release() noexcept { return std::exchange(handle, 0); }
};

std::string shader_log(GLuint shader)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return log;
}

std::string program_log(GLuint program)
{
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    return log;
}

GLuint compile_shader(
    GLenum type,
    std::string_view source,
    std::string_view stage)
{
    if (source.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())) {
        throw std::length_error(std::string(stage) + " shader source is too long");
    }

    ShaderObject shader{glCreateShader(type)};
    if (shader.handle == 0) {
        throw std::runtime_error(std::string(stage) + " shader creation failed");
    }

    const char* data = source.data();
    const GLint length = static_cast<GLint>(source.size());
    glShaderSource(shader.handle, 1, &data, &length);
    glCompileShader(shader.handle);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader.handle, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        const std::string log = shader_log(shader.handle);
        throw std::runtime_error(
            std::string(stage) + " shader compilation failed: " + log);
    }
    return shader.release();
}

GLuint link_program(std::initializer_list<GLuint> shaders)
{
    ProgramObject program{glCreateProgram()};
    if (program.handle == 0) {
        throw std::runtime_error("shader program creation failed");
    }

    for (const GLuint shader : shaders) {
        glAttachShader(program.handle, shader);
    }
    glLinkProgram(program.handle);

    GLint linked = GL_FALSE;
    glGetProgramiv(program.handle, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        const std::string log = program_log(program.handle);
        throw std::runtime_error("shader program linking failed: " + log);
    }
    return program.release();
}

}

ShaderProgram::ShaderProgram(unsigned int handle) noexcept : handle_(handle) {}

ShaderProgram ShaderProgram::graphics(
    std::string_view vertex_source,
    std::string_view fragment_source)
{
    ShaderObject vertex{
        compile_shader(GL_VERTEX_SHADER, vertex_source, "vertex")};
    ShaderObject fragment{
        compile_shader(GL_FRAGMENT_SHADER, fragment_source, "fragment")};
    return ShaderProgram(link_program({vertex.handle, fragment.handle}));
}

ShaderProgram ShaderProgram::compute(std::string_view source)
{
    ShaderObject shader{compile_shader(GL_COMPUTE_SHADER, source, "compute")};
    return ShaderProgram(link_program({shader.handle}));
}

ShaderProgram::~ShaderProgram()
{
    if (handle_ != 0) {
        glDeleteProgram(handle_);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : handle_(std::exchange(other.handle_, 0)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept
{
    if (this != &other) {
        if (handle_ != 0) {
            glDeleteProgram(handle_);
        }
        handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
}

void ShaderProgram::use() const noexcept
{
    glUseProgram(handle_);
}

int ShaderProgram::uniform_location(const char* name) const noexcept
{
    return glGetUniformLocation(handle_, name);
}

}
