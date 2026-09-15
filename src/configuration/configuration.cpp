module;

#include <cstddef>
#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>

module hw3d.configuration;

namespace hw3d {
namespace {

constexpr std::int64_t max_triangle_count = 1'000'000;

Triangle read_triangle(std::istream& input, const std::size_t index)
{
    Triangle triangle{};

    if (!(input >> triangle.a.x >> triangle.a.y >> triangle.a.z
                >> triangle.b.x >> triangle.b.y >> triangle.b.z
                >> triangle.c.x >> triangle.c.y >> triangle.c.z)) {
        throw std::runtime_error(
            "failed to read triangle " + std::to_string(index));
    }

    return triangle;
}

}

Configuration read_configuration(std::istream& input)
{
    std::int64_t triangle_count{};
    if (!(input >> triangle_count)) {
        throw std::runtime_error("failed to read triangle count");
    }

    if (triangle_count <= 0 || triangle_count >= max_triangle_count) {
        throw std::runtime_error(
            "triangle count must be in the range (0, 1000000)");
    }

    Configuration configuration;
    configuration.triangles.reserve(
        static_cast<std::size_t>(triangle_count));

    for (std::int64_t index = 0; index < triangle_count; ++index) {
        configuration.triangles.push_back(
            read_triangle(input, static_cast<std::size_t>(index + 1)));
    }

    return configuration;
}

}
