module;

#include <cstddef>
#include <cstdint>
#include <charconv>
#include <chrono>
#include <cmath>
#include <istream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <system_error>

module hw3d.configuration;

namespace hw3d {
namespace {

constexpr std::int64_t max_triangle_count = 1'000'000;

std::chrono::seconds read_simulation_duration(std::istream& input)
{
    std::string token;
    input >> token;
    if (!input) {
        throw std::runtime_error("failed to read simulation duration");
    }

    std::chrono::seconds::rep seconds{};
    const char* begin = token.data();
    const char* const end = begin + token.size();
    if (begin != end && *begin == '+') {
        ++begin;
    }
    const auto [position, error] = std::from_chars(begin, end, seconds);
    if (error != std::errc{} || position != end) {
        throw std::runtime_error("failed to read simulation duration");
    }
    if (seconds <= 0) {
        throw std::runtime_error("simulation duration must be positive");
    }
    return std::chrono::seconds{seconds};
}

Vec3 read_point(
    std::istream& input,
    const std::size_t index,
    const char* const description)
{
    Vec3 point{};
    input >> point.x >> point.y >> point.z;
    if (!input) {
        throw std::runtime_error(
            "failed to read " + std::string(description)
            + " for triangle " + std::to_string(index));
    }
    return point;
}

bool finite(const Vec3& value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

void read_rotation(
    std::istream& input,
    InitialTriangle& triangle,
    const std::size_t index)
{
    const Vec3 first = read_point(input, index, "rotation axis");
    const Vec3 second = read_point(input, index, "rotation axis");

    const double x = static_cast<double>(second.x) - first.x;
    const double y = static_cast<double>(second.y) - first.y;
    const double z = static_cast<double>(second.z) - first.z;
    const double length = std::hypot(x, y, z);
    if (!finite(first) || !finite(second)
        || !std::isfinite(length) || length == 0.0) {
        throw std::runtime_error(
            "rotation axis for triangle " + std::to_string(index)
            + " must use distinct finite points");
    }

    triangle.rotation_axis_origin = first;
    triangle.rotation_axis_direction = {
        static_cast<float>(x / length),
        static_cast<float>(y / length),
        static_cast<float>(z / length),
    };

    float degrees_per_second{};
    input >> degrees_per_second;
    if (!input) {
        throw std::runtime_error(
            "failed to read angular speed for triangle "
            + std::to_string(index));
    }
    if (!std::isfinite(degrees_per_second)) {
        throw std::runtime_error(
            "angular speed for triangle " + std::to_string(index)
            + " must be finite");
    }

    if (degrees_per_second == 0.0F) {
        triangle.angular_velocity_radians_per_second = 0.0F;
    } else {
        constexpr float degrees_to_radians =
            std::numbers::pi_v<float> / 180.0F;
        const float radians_per_second =
            -degrees_per_second * degrees_to_radians;
        if (!std::isfinite(radians_per_second)) {
            throw std::runtime_error(
                "angular speed for triangle " + std::to_string(index)
                + " is too large after conversion to radians");
        }
        triangle.angular_velocity_radians_per_second = radians_per_second;
    }
}

InitialTriangle read_triangle(std::istream& input, const std::size_t index)
{
    InitialTriangle triangle{};
    input >> triangle.a.x >> triangle.a.y >> triangle.a.z
          >> triangle.b.x >> triangle.b.y >> triangle.b.z
          >> triangle.c.x >> triangle.c.y >> triangle.c.z;
    if (!input) {
        throw std::runtime_error(
            "failed to read triangle " + std::to_string(index));
    }

    read_rotation(input, triangle, index);

    return triangle;
}

}

Configuration read_configuration(std::istream& input)
{
    std::int64_t triangle_count{};
    input >> triangle_count;
    if (!input) {
        throw std::runtime_error("failed to read triangle count");
    }

    if (triangle_count <= 0 || triangle_count >= max_triangle_count) {
        throw std::runtime_error(
            "triangle count must be in the range (0, 1000000)");
    }

    Configuration configuration;
    configuration.simulation_duration = read_simulation_duration(input);
    configuration.triangles.reserve(
        static_cast<std::size_t>(triangle_count));

    for (std::int64_t index = 0; index < triangle_count; ++index) {
        configuration.triangles.push_back(
            read_triangle(input, static_cast<std::size_t>(index + 1)));
    }
    return configuration;
}

}
