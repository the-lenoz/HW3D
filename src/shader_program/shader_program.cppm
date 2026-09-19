module;

#include <string_view>

export module hw3d.shader_program;

export namespace hw3d {

class ShaderProgram final {
public:
    [[nodiscard]] static ShaderProgram graphics(
        std::string_view vertex_source,
        std::string_view fragment_source);
    [[nodiscard]] static ShaderProgram compute(std::string_view source);

    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    void use() const noexcept;
    [[nodiscard]] int uniform_location(const char* name) const noexcept;

private:
    explicit ShaderProgram(unsigned int handle) noexcept;
    unsigned int handle_ = 0;
};

}
