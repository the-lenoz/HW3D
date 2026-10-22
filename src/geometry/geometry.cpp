module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <numbers>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

module hw3d.geometry;

namespace hw3d {
namespace {

std::int64_t voxel_coordinate(
    const float value,
    const float origin,
    const float voxel_size) noexcept
{
    constexpr std::int64_t coordinate_limit =
        std::numeric_limits<std::int64_t>::max() / 4;
    const float scaled = (value - origin) / voxel_size;

    if (!std::isfinite(scaled)
        || scaled >= static_cast<float>(coordinate_limit)) {
        return coordinate_limit;
    }
    if (scaled <= static_cast<float>(-coordinate_limit)) {
        return -coordinate_limit;
    }
    return static_cast<std::int64_t>(std::floor(scaled));
}

struct DVec3 {
    double x;
    double y;
    double z;
};

struct DoubleVoxelGrid {
    DVec3 origin;
    DVec3 voxel_size;
};

bool finite(const Vec3& point) noexcept
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z);
}

bool valid(const Aabb& bounds) noexcept
{
    return finite(bounds.minimum)
        && finite(bounds.maximum)
        && bounds.minimum.x <= bounds.maximum.x
        && bounds.minimum.y <= bounds.maximum.y
        && bounds.minimum.z <= bounds.maximum.z;
}

DoubleVoxelGrid make_double_voxel_grid(
    const std::span<const Aabb> bounds) noexcept
{
    if (bounds.empty()) {
        return {{}, {1.0, 1.0, 1.0}};
    }

    DVec3 origin{
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
    };
    DVec3 voxel_size{};
    for (const Aabb& value : bounds) {
        origin.x = std::min(origin.x, static_cast<double>(value.minimum.x));
        origin.y = std::min(origin.y, static_cast<double>(value.minimum.y));
        origin.z = std::min(origin.z, static_cast<double>(value.minimum.z));
        voxel_size.x = std::max(
            voxel_size.x,
            static_cast<double>(value.maximum.x) - value.minimum.x);
        voxel_size.y = std::max(
            voxel_size.y,
            static_cast<double>(value.maximum.y) - value.minimum.y);
        voxel_size.z = std::max(
            voxel_size.z,
            static_cast<double>(value.maximum.z) - value.minimum.z);
    }
    const double fallback_size = std::max({
        voxel_size.x, voxel_size.y, voxel_size.z, 1.0,
    });
    if (voxel_size.x == 0.0) voxel_size.x = fallback_size;
    if (voxel_size.y == 0.0) voxel_size.y = fallback_size;
    if (voxel_size.z == 0.0) voxel_size.z = fallback_size;
    return {origin, voxel_size};
}

std::int64_t double_voxel_coordinate(
    const double value,
    const double origin,
    const double voxel_size) noexcept
{
    constexpr std::int64_t coordinate_limit =
        std::numeric_limits<std::int64_t>::max() / 4;
    const double scaled = (value - origin) / voxel_size;
    if (!std::isfinite(scaled)
        || scaled >= static_cast<double>(coordinate_limit)) {
        return coordinate_limit;
    }
    if (scaled <= static_cast<double>(-coordinate_limit)) {
        return -coordinate_limit;
    }
    return static_cast<std::int64_t>(std::floor(scaled));
}

Voxel double_voxel_for(
    const Aabb& bounds,
    const DoubleVoxelGrid& grid) noexcept
{
    const DVec3 bounds_center{
        static_cast<double>(bounds.minimum.x)
            + (static_cast<double>(bounds.maximum.x) - bounds.minimum.x) * 0.5,
        static_cast<double>(bounds.minimum.y)
            + (static_cast<double>(bounds.maximum.y) - bounds.minimum.y) * 0.5,
        static_cast<double>(bounds.minimum.z)
            + (static_cast<double>(bounds.maximum.z) - bounds.minimum.z) * 0.5,
    };
    return {
        double_voxel_coordinate(
            bounds_center.x, grid.origin.x, grid.voxel_size.x),
        double_voxel_coordinate(
            bounds_center.y, grid.origin.y, grid.voxel_size.y),
        double_voxel_coordinate(
            bounds_center.z, grid.origin.z, grid.voxel_size.z),
    };
}

float outward_min(const double value) noexcept
{
    float result = static_cast<float>(value);
    if (static_cast<double>(result) > value) {
        result = std::nextafter(
            result,
            -std::numeric_limits<float>::infinity());
    }
    return result;
}

float outward_max(const double value) noexcept
{
    float result = static_cast<float>(value);
    if (static_cast<double>(result) < value) {
        result = std::nextafter(
            result,
            std::numeric_limits<float>::infinity());
    }
    return result;
}

using RotationMatrix = std::array<std::array<double, 3>, 3>;

RotationMatrix rotation_matrix(
    const DVec3& axis,
    const double angle) noexcept
{
    const double x = axis.x;
    const double y = axis.y;
    const double z = axis.z;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const double complement = 1.0 - cosine;

    return {{
        {{cosine + x * x * complement,
          x * y * complement - z * sine,
          x * z * complement + y * sine}},
        {{y * x * complement + z * sine,
          cosine + y * y * complement,
          y * z * complement - x * sine}},
        {{z * x * complement - y * sine,
          z * y * complement + x * sine,
          cosine + z * z * complement}},
    }};
}

DVec3 normalized_rotation_axis(const Vec3& axis)
{
    const double length = std::hypot(
        static_cast<double>(axis.x),
        static_cast<double>(axis.y),
        static_cast<double>(axis.z));
    if (!std::isfinite(length) || length == 0.0) {
        throw std::invalid_argument(
            "rotation axis direction must be finite and non-zero");
    }
    return {
        static_cast<double>(axis.x) / length,
        static_cast<double>(axis.y) / length,
        static_cast<double>(axis.z) / length,
    };
}

Vec3 rotate_vertex(
    const Vec3& vertex,
    const Vec3& origin,
    const RotationMatrix& matrix) noexcept
{
    const std::array<double, 3> radius{
        static_cast<double>(vertex.x) - origin.x,
        static_cast<double>(vertex.y) - origin.y,
        static_cast<double>(vertex.z) - origin.z,
    };

    return {
        static_cast<float>(origin.x
            + std::inner_product(
                matrix[0].begin(), matrix[0].end(), radius.begin(), 0.0)),
        static_cast<float>(origin.y
            + std::inner_product(
                matrix[1].begin(), matrix[1].end(), radius.begin(), 0.0)),
        static_cast<float>(origin.z
            + std::inner_product(
                matrix[2].begin(), matrix[2].end(), radius.begin(), 0.0)),
    };
}

}

void update_vertices(
    const std::span<const InitialTriangle> initial,
    const std::span<Triangle> current,
    const std::chrono::duration<double> elapsed)
{
    const double elapsed_seconds = elapsed.count();
    if (initial.size() != current.size()) {
        throw std::invalid_argument(
            "initial and current triangle counts must match");
    }
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0) {
        throw std::invalid_argument(
            "elapsed simulation time must be finite and non-negative");
    }

    constexpr double full_turn = 2.0 * std::numbers::pi_v<double>;
    for (std::size_t index = 0; index < initial.size(); ++index) {
        const InitialTriangle& source = initial[index];
        Triangle& destination = current[index];

        if (source.angular_velocity_radians_per_second == 0.0F) {
            destination.a = source.a;
            destination.b = source.b;
            destination.c = source.c;
            continue;
        }

        if (!finite(source.rotation_axis_origin)) {
            throw std::invalid_argument(
                "rotation axis origin must be finite");
        }
        const DVec3 normalized_axis = normalized_rotation_axis(
            source.rotation_axis_direction);

        const double angle = std::remainder(
            static_cast<double>(
                source.angular_velocity_radians_per_second)
                * elapsed_seconds,
            full_turn);
        if (angle == 0.0) {
            destination.a = source.a;
            destination.b = source.b;
            destination.c = source.c;
            continue;
        }

        const RotationMatrix matrix = rotation_matrix(
            normalized_axis,
            angle);
        destination.a = rotate_vertex(
            source.a, source.rotation_axis_origin, matrix);
        destination.b = rotate_vertex(
            source.b, source.rotation_axis_origin, matrix);
        destination.c = rotate_vertex(
            source.c, source.rotation_axis_origin, matrix);
    }
}

std::size_t VoxelHash::operator()(const Voxel& voxel) const noexcept
{
    auto mix = [](std::uint64_t value) noexcept {
        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    };

    const std::uint64_t x = mix(static_cast<std::uint64_t>(voxel.x));
    const std::uint64_t y = mix(static_cast<std::uint64_t>(voxel.y));
    const std::uint64_t z = mix(static_cast<std::uint64_t>(voxel.z));
    return static_cast<std::size_t>(x ^ (y << 1U) ^ (z << 7U));
}

Aabb aabb(const Triangle& triangle) noexcept
{
    return {
        .minimum = {
            std::min({triangle.a.x, triangle.b.x, triangle.c.x}),
            std::min({triangle.a.y, triangle.b.y, triangle.c.y}),
            std::min({triangle.a.z, triangle.b.z, triangle.c.z}),
        },
        .maximum = {
            std::max({triangle.a.x, triangle.b.x, triangle.c.x}),
            std::max({triangle.a.y, triangle.b.y, triangle.c.y}),
            std::max({triangle.a.z, triangle.b.z, triangle.c.z}),
        },
    };
}

Aabb motion_aabb(const InitialTriangle& triangle) noexcept
{
    const Aabb static_bounds{
        .minimum = {
            std::min({triangle.a.x, triangle.b.x, triangle.c.x}),
            std::min({triangle.a.y, triangle.b.y, triangle.c.y}),
            std::min({triangle.a.z, triangle.b.z, triangle.c.z}),
        },
        .maximum = {
            std::max({triangle.a.x, triangle.b.x, triangle.c.x}),
            std::max({triangle.a.y, triangle.b.y, triangle.c.y}),
            std::max({triangle.a.z, triangle.b.z, triangle.c.z}),
        },
    };
    if (triangle.angular_velocity_radians_per_second == 0.0F) {
        return static_bounds;
    }

    const double direction_x = triangle.rotation_axis_direction.x;
    const double direction_y = triangle.rotation_axis_direction.y;
    const double direction_z = triangle.rotation_axis_direction.z;
    const double direction_length = std::hypot(
        direction_x,
        direction_y,
        direction_z);
    if (!std::isfinite(direction_length) || direction_length == 0.0) {
        return static_bounds;
    }

    const double u_x = direction_x / direction_length;
    const double u_y = direction_y / direction_length;
    const double u_z = direction_z / direction_length;
    const std::array<double, 3> projectors{
        std::sqrt(std::max(0.0, 1.0 - u_x * u_x)),
        std::sqrt(std::max(0.0, 1.0 - u_y * u_y)),
        std::sqrt(std::max(0.0, 1.0 - u_z * u_z)),
    };
    const std::array<double, 3> origin{
        triangle.rotation_axis_origin.x,
        triangle.rotation_axis_origin.y,
        triangle.rotation_axis_origin.z,
    };
    if (!std::ranges::all_of(origin, [](const double value) {
            return std::isfinite(value);
        })) {
        return static_bounds;
    }
    const std::array<Vec3, 3> vertices{
        triangle.a,
        triangle.b,
        triangle.c,
    };

    std::array<double, 3> minimum{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
    };
    std::array<double, 3> maximum{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };

    for (const Vec3& vertex : vertices) {
        const std::array<double, 3> relative{
            static_cast<double>(vertex.x) - origin[0],
            static_cast<double>(vertex.y) - origin[1],
            static_cast<double>(vertex.z) - origin[2],
        };
        const double height = relative[0] * u_x
            + relative[1] * u_y
            + relative[2] * u_z;
        const std::array<double, 3> circle_center{
            origin[0] + height * u_x,
            origin[1] + height * u_y,
            origin[2] + height * u_z,
        };
        const double distance_squared = std::max(
            0.0,
            relative[0] * relative[0]
                + relative[1] * relative[1]
                + relative[2] * relative[2]
                - height * height);
        const double radius = std::sqrt(distance_squared);

        for (std::size_t coordinate = 0;
             coordinate < minimum.size();
             ++coordinate) {
            const double projected_radius = radius * projectors[coordinate];
            minimum[coordinate] = std::min(
                minimum[coordinate],
                circle_center[coordinate] - projected_radius);
            maximum[coordinate] = std::max(
                maximum[coordinate],
                circle_center[coordinate] + projected_radius);
        }
    }

    return {
        .minimum = {
            outward_min(minimum[0]),
            outward_min(minimum[1]),
            outward_min(minimum[2]),
        },
        .maximum = {
            outward_max(maximum[0]),
            outward_max(maximum[1]),
            outward_max(maximum[2]),
        },
    };
}

void calculate_motion_bounds(
    const std::span<const InitialTriangle> initial,
    const std::span<Aabb> output)
{
    if (initial.size() != output.size()) {
        throw std::invalid_argument(
            "initial triangle and motion bound counts must match");
    }

    const auto finite_point = [](const Vec3& point) noexcept {
        return std::isfinite(point.x)
            && std::isfinite(point.y)
            && std::isfinite(point.z);
    };
    const auto valid_bounds = [&](const Aabb& bounds) noexcept {
        return finite_point(bounds.minimum)
            && finite_point(bounds.maximum)
            && bounds.minimum.x <= bounds.maximum.x
            && bounds.minimum.y <= bounds.maximum.y
            && bounds.minimum.z <= bounds.maximum.z;
    };

    for (std::size_t index = 0; index < initial.size(); ++index) {
        const InitialTriangle& triangle = initial[index];
        if (!finite_point(triangle.a)
            || !finite_point(triangle.b)
            || !finite_point(triangle.c)
            || !finite_point(triangle.rotation_axis_origin)
            || !finite_point(triangle.rotation_axis_direction)
            || !std::isfinite(
                triangle.angular_velocity_radians_per_second)) {
            throw std::invalid_argument(
                "initial triangle geometry must be finite");
        }
        const Vec3& direction = triangle.rotation_axis_direction;
        const double direction_length = std::hypot(
            static_cast<double>(direction.x),
            static_cast<double>(direction.y),
            static_cast<double>(direction.z));
        if (!std::isfinite(direction_length) || direction_length == 0.0) {
            throw std::invalid_argument(
                "rotation axis direction must be non-zero");
        }
        output[index] = motion_aabb(triangle);
        if (!valid_bounds(output[index])) {
            throw std::invalid_argument(
                "motion bounds must be finite and ordered");
        }
    }
}

Vec3 extent(const Aabb& bounds) noexcept
{
    return {
        bounds.maximum.x - bounds.minimum.x,
        bounds.maximum.y - bounds.minimum.y,
        bounds.maximum.z - bounds.minimum.z,
    };
}

Vec3 center(const Aabb& bounds) noexcept
{
    return {
        std::midpoint(bounds.minimum.x, bounds.maximum.x),
        std::midpoint(bounds.minimum.y, bounds.maximum.y),
        std::midpoint(bounds.minimum.z, bounds.maximum.z),
    };
}

VoxelGrid make_voxel_grid(std::span<const Aabb> bounds) noexcept
{
    if (bounds.empty()) {
        return {{}, {1.0F, 1.0F, 1.0F}};
    }

    Vec3 origin{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    Vec3 voxel_size{};

    for (const Aabb& value : bounds) {
        origin.x = std::min(origin.x, value.minimum.x);
        origin.y = std::min(origin.y, value.minimum.y);
        origin.z = std::min(origin.z, value.minimum.z);

        const Vec3 value_extent = extent(value);
        voxel_size.x = std::max(voxel_size.x, value_extent.x);
        voxel_size.y = std::max(voxel_size.y, value_extent.y);
        voxel_size.z = std::max(voxel_size.z, value_extent.z);
    }

    const float fallback_size = std::max({
        voxel_size.x,
        voxel_size.y,
        voxel_size.z,
        1.0F,
    });
    if (voxel_size.x == 0.0F) {
        voxel_size.x = fallback_size;
    }
    if (voxel_size.y == 0.0F) {
        voxel_size.y = fallback_size;
    }
    if (voxel_size.z == 0.0F) {
        voxel_size.z = fallback_size;
    }

    return {origin, voxel_size};
}

Voxel voxel_for(const Vec3& point, const VoxelGrid& grid) noexcept
{
    return {
        voxel_coordinate(point.x, grid.origin.x, grid.voxel_size.x),
        voxel_coordinate(point.y, grid.origin.y, grid.voxel_size.y),
        voxel_coordinate(point.z, grid.origin.z, grid.voxel_size.z),
    };
}

MotionPairCandidates make_motion_pair_candidates(
    const std::span<const Aabb> motion_bounds,
    const std::size_t maximum_pair_count)
{
    if (!std::ranges::all_of(motion_bounds, valid)) {
        throw std::invalid_argument(
            "motion bounds must be finite and ordered");
    }
    if (motion_bounds.size()
        > static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        throw std::length_error(
            "too many triangles for 32-bit intersection pair indices");
    }

    MotionPairCandidates result;
    result.voxels.resize(motion_bounds.size());
    result.pairs.emplace();

    const DoubleVoxelGrid voxel_grid =
        make_double_voxel_grid(motion_bounds);
    std::unordered_map<Voxel, std::vector<std::uint32_t>, VoxelHash> grid;
    grid.reserve(motion_bounds.size());
    for (std::size_t id = 0; id < motion_bounds.size(); ++id) {
        const Voxel voxel = double_voxel_for(motion_bounds[id], voxel_grid);
        result.voxels[id] = voxel;
        grid[voxel].push_back(static_cast<std::uint32_t>(id));
    }

    auto append_pair = [&](const TrianglePair pair) {
        std::vector<TrianglePair>& pairs = *result.pairs;
        if (pairs.size() >= maximum_pair_count) {
            result.pairs.reset();
            return false;
        }
        if (pairs.size() == pairs.capacity()) {
            constexpr std::size_t initial_capacity = 1024;
            const std::size_t doubled = pairs.capacity() == 0
                ? initial_capacity
                : pairs.capacity() > maximum_pair_count / 2
                    ? maximum_pair_count
                    : pairs.capacity() * 2;
            const std::size_t next_capacity = std::min(
                maximum_pair_count,
                std::max(pairs.size() + 1, doubled));
            try {
                pairs.reserve(next_capacity);
            } catch (const std::bad_alloc&) {
                result.pairs.reset();
                return false;
            }
        }
        pairs.push_back(pair);
        return true;
    };

    for (std::size_t first = 0; first < motion_bounds.size(); ++first) {
        const Voxel voxel = result.voxels[first];
        for (std::int64_t x_offset = -1; x_offset <= 1; ++x_offset) {
            for (std::int64_t y_offset = -1; y_offset <= 1; ++y_offset) {
                for (std::int64_t z_offset = -1; z_offset <= 1; ++z_offset) {
                    const Voxel neighbor{
                        voxel.x + x_offset,
                        voxel.y + y_offset,
                        voxel.z + z_offset,
                    };
                    const auto bucket = grid.find(neighbor);
                    if (bucket == grid.end()) continue;
                    for (const std::uint32_t second : bucket->second) {
                        if (second <= first
                            || !aabbs_intersect(
                                motion_bounds[first],
                                motion_bounds[second])) {
                            continue;
                        }
                        if (!append_pair({
                                static_cast<std::uint32_t>(first),
                                second,
                            })) {
                            return result;
                        }
                    }
                }
            }
        }
    }
    return result;
}

bool is_degenerate(const Triangle& triangle)
{
    const Vector2<float> a_xy{triangle.a.x, triangle.a.y};
    const Vector2<float> b_xy{triangle.b.x, triangle.b.y};
    const Vector2<float> c_xy{triangle.c.x, triangle.c.y};
    if (orient2d(a_xy, b_xy, c_xy) != Sign::zero) {
        return false;
    }

    const Vector2<float> a_xz{triangle.a.x, triangle.a.z};
    const Vector2<float> b_xz{triangle.b.x, triangle.b.z};
    const Vector2<float> c_xz{triangle.c.x, triangle.c.z};
    if (orient2d(a_xz, b_xz, c_xz) != Sign::zero) {
        return false;
    }

    const Vector2<float> a_yz{triangle.a.y, triangle.a.z};
    const Vector2<float> b_yz{triangle.b.y, triangle.b.z};
    const Vector2<float> c_yz{triangle.c.y, triangle.c.z};
    return orient2d(a_yz, b_yz, c_yz) == Sign::zero;
}

}
