module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

module hw3d.cpu_intersections;

import hw3d.geometry;

namespace hw3d {
namespace {

struct IndexedTriangle {
    std::size_t id;
    Aabb bounds;
    Voxel voxel;
};

struct Vec2 {
    float x;
    float y;
};

struct Interval {
    float minimum;
    float maximum;
};

enum class PrimitiveKind {
    point,
    segment,
    triangle,
};

struct Primitive {
    PrimitiveKind kind;
    Vec3 first;
    Vec3 second;
};

Vec3 add(const Vec3& left, const Vec3& right) noexcept
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 subtract(const Vec3& left, const Vec3& right) noexcept
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
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

float length_squared(const Vec3& vector) noexcept
{
    return dot(vector, vector);
}

float length(const Vec3& vector) noexcept
{
    return std::sqrt(length_squared(vector));
}

Vec3 normalize(const Vec3& vector) noexcept
{
    return scale(vector, 1.0F / length(vector));
}

std::array<Vec3, 3> vertices(const Triangle& triangle) noexcept
{
    return {triangle.a, triangle.b, triangle.c};
}

float coordinate_scale(
    const Triangle& left,
    const Triangle& right) noexcept
{
    float result = 1.0F;
    for (const Vec3 point : vertices(left)) {
        result = std::max({
            result,
            std::abs(point.x),
            std::abs(point.y),
            std::abs(point.z),
        });
    }
    for (const Vec3 point : vertices(right)) {
        result = std::max({
            result,
            std::abs(point.x),
            std::abs(point.y),
            std::abs(point.z),
        });
    }
    return result;
}

float collision_tolerance(
    const Triangle& left,
    const Triangle& right) noexcept
{
    return 32.0F * std::numeric_limits<float>::epsilon()
        * coordinate_scale(left, right);
}

std::size_t dominant_axis(const Vec3& normal) noexcept
{
    const Vec3 absolute{
        std::abs(normal.x),
        std::abs(normal.y),
        std::abs(normal.z),
    };

    if (absolute.x >= absolute.y && absolute.x >= absolute.z) {
        return 0;
    }
    if (absolute.y >= absolute.z) {
        return 1;
    }
    return 2;
}

Vec2 project_2d(const Vec3& point, const std::size_t dropped_axis) noexcept
{
    if (dropped_axis == 0) {
        return {point.y, point.z};
    }
    if (dropped_axis == 1) {
        return {point.x, point.z};
    }
    return {point.x, point.y};
}

float orientation(
    const Vec2& first,
    const Vec2& second,
    const Vec2& third) noexcept
{
    return (second.x - first.x) * (third.y - first.y)
        - (second.y - first.y) * (third.x - first.x);
}

float segment_length_2d(const Vec2& first, const Vec2& second) noexcept
{
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    return std::sqrt(x * x + y * y);
}

bool point_on_segment_2d(
    const Vec2& point,
    const Vec2& first,
    const Vec2& second,
    const float tolerance) noexcept
{
    const float orientation_tolerance = tolerance
        * std::max(segment_length_2d(first, second), 1.0F);
    return std::abs(orientation(first, second, point))
            <= orientation_tolerance
        && point.x >= std::min(first.x, second.x) - tolerance
        && point.x <= std::max(first.x, second.x) + tolerance
        && point.y >= std::min(first.y, second.y) - tolerance
        && point.y <= std::max(first.y, second.y) + tolerance;
}

bool segments_intersect_2d(
    const Vec2& first_a,
    const Vec2& second_a,
    const Vec2& first_b,
    const Vec2& second_b,
    const float tolerance) noexcept
{
    const float a = orientation(first_a, second_a, first_b);
    const float b = orientation(first_a, second_a, second_b);
    const float c = orientation(first_b, second_b, first_a);
    const float d = orientation(first_b, second_b, second_a);
    const float tolerance_a = tolerance
        * std::max(segment_length_2d(first_a, second_a), 1.0F);
    const float tolerance_b = tolerance
        * std::max(segment_length_2d(first_b, second_b), 1.0F);

    const bool crosses_a = (a > tolerance_a && b < -tolerance_a)
        || (a < -tolerance_a && b > tolerance_a);
    const bool crosses_b = (c > tolerance_b && d < -tolerance_b)
        || (c < -tolerance_b && d > tolerance_b);
    if (crosses_a && crosses_b) {
        return true;
    }

    return point_on_segment_2d(first_b, first_a, second_a, tolerance)
        || point_on_segment_2d(second_b, first_a, second_a, tolerance)
        || point_on_segment_2d(first_a, first_b, second_b, tolerance)
        || point_on_segment_2d(second_a, first_b, second_b, tolerance);
}

bool point_in_triangle_2d(
    const Vec2& point,
    const std::array<Vec2, 3>& triangle,
    const float tolerance) noexcept
{
    const float first = orientation(triangle[0], triangle[1], point);
    const float second = orientation(triangle[1], triangle[2], point);
    const float third = orientation(triangle[2], triangle[0], point);
    const float orientation_tolerance = tolerance * std::max({
        segment_length_2d(triangle[0], triangle[1]),
        segment_length_2d(triangle[1], triangle[2]),
        segment_length_2d(triangle[2], triangle[0]),
        1.0F,
    });

    const bool has_negative = first < -orientation_tolerance
        || second < -orientation_tolerance
        || third < -orientation_tolerance;
    const bool has_positive = first > orientation_tolerance
        || second > orientation_tolerance
        || third > orientation_tolerance;
    return !(has_negative && has_positive);
}

std::array<Vec2, 3> project_triangle(
    const Triangle& triangle,
    const std::size_t dropped_axis) noexcept
{
    return {
        project_2d(triangle.a, dropped_axis),
        project_2d(triangle.b, dropped_axis),
        project_2d(triangle.c, dropped_axis),
    };
}

bool coplanar_triangles_intersect(
    const Triangle& left,
    const Triangle& right,
    const Vec3& normal,
    const float tolerance) noexcept
{
    const std::size_t dropped_axis = dominant_axis(normal);
    const auto left_2d = project_triangle(left, dropped_axis);
    const auto right_2d = project_triangle(right, dropped_axis);

    for (std::size_t left_edge = 0; left_edge < 3; ++left_edge) {
        for (std::size_t right_edge = 0; right_edge < 3; ++right_edge) {
            if (segments_intersect_2d(
                    left_2d[left_edge],
                    left_2d[(left_edge + 1) % 3],
                    right_2d[right_edge],
                    right_2d[(right_edge + 1) % 3],
                    tolerance)) {
                return true;
            }
        }
    }

    return point_in_triangle_2d(left_2d[0], right_2d, tolerance)
        || point_in_triangle_2d(right_2d[0], left_2d, tolerance);
}

Primitive primitive(const Triangle& triangle, const float tolerance) noexcept
{
    const std::array<Vec3, 3> points = vertices(triangle);
    const std::array<std::array<std::size_t, 2>, 3> edges{{
        {0, 1},
        {1, 2},
        {2, 0},
    }};

    float longest_squared{};
    std::array<std::size_t, 2> longest_edge{0, 0};
    for (const auto edge : edges) {
        const float squared = length_squared(
            subtract(points[edge[1]], points[edge[0]]));
        if (squared > longest_squared) {
            longest_squared = squared;
            longest_edge = edge;
        }
    }

    if (longest_squared <= tolerance * tolerance) {
        return {PrimitiveKind::point, triangle.a, triangle.a};
    }

    const Vec3 normal = cross(
        subtract(triangle.b, triangle.a),
        subtract(triangle.c, triangle.a));
    if (length(normal) <= tolerance * std::sqrt(longest_squared)) {
        return {
            PrimitiveKind::segment,
            points[longest_edge[0]],
            points[longest_edge[1]],
        };
    }

    return {PrimitiveKind::triangle, {}, {}};
}

float point_segment_distance_squared(
    const Vec3& point,
    const Vec3& first,
    const Vec3& second) noexcept
{
    const Vec3 segment = subtract(second, first);
    const float denominator = length_squared(segment);
    if (denominator == 0.0F) {
        return length_squared(subtract(point, first));
    }

    const float parameter = std::clamp(
        dot(subtract(point, first), segment) / denominator,
        0.0F,
        1.0F);
    return length_squared(
        subtract(point, add(first, scale(segment, parameter))));
}

float segment_segment_distance_squared(
    const Vec3& first_a,
    const Vec3& second_a,
    const Vec3& first_b,
    const Vec3& second_b) noexcept
{
    const Vec3 direction_a = subtract(second_a, first_a);
    const Vec3 direction_b = subtract(second_b, first_b);
    const Vec3 offset = subtract(first_a, first_b);
    const float a = dot(direction_a, direction_a);
    const float b = dot(direction_a, direction_b);
    const float c = dot(direction_b, direction_b);
    const float d = dot(direction_a, offset);
    const float e = dot(direction_b, offset);
    const float denominator = a * c - b * b;

    float s{};
    if (denominator > std::numeric_limits<float>::epsilon()) {
        s = std::clamp((b * e - c * d) / denominator, 0.0F, 1.0F);
    }

    float t = c > 0.0F ? (b * s + e) / c : 0.0F;
    if (t < 0.0F) {
        t = 0.0F;
        s = a > 0.0F ? std::clamp(-d / a, 0.0F, 1.0F) : 0.0F;
    } else if (t > 1.0F) {
        t = 1.0F;
        s = a > 0.0F
            ? std::clamp((b - d) / a, 0.0F, 1.0F)
            : 0.0F;
    }

    const Vec3 closest_a = add(first_a, scale(direction_a, s));
    const Vec3 closest_b = add(first_b, scale(direction_b, t));
    return length_squared(subtract(closest_a, closest_b));
}

bool point_on_triangle(
    const Vec3& point,
    const Triangle& triangle,
    const Vec3& normal,
    const float tolerance) noexcept
{
    const Vec3 unit_normal = normalize(normal);
    if (std::abs(dot(unit_normal, subtract(point, triangle.a))) > tolerance) {
        return false;
    }

    const std::size_t dropped_axis = dominant_axis(unit_normal);
    return point_in_triangle_2d(
        project_2d(point, dropped_axis),
        project_triangle(triangle, dropped_axis),
        tolerance);
}

bool segment_intersects_triangle(
    const Vec3& first,
    const Vec3& second,
    const Triangle& triangle,
    const Vec3& triangle_normal,
    const float tolerance) noexcept
{
    const Vec3 unit_normal = normalize(triangle_normal);
    const float first_distance = dot(
        unit_normal,
        subtract(first, triangle.a));
    const float second_distance = dot(
        unit_normal,
        subtract(second, triangle.a));

    if ((first_distance > tolerance && second_distance > tolerance)
        || (first_distance < -tolerance && second_distance < -tolerance)) {
        return false;
    }

    const std::size_t dropped_axis = dominant_axis(unit_normal);
    const auto triangle_2d = project_triangle(triangle, dropped_axis);
    if (std::abs(first_distance) <= tolerance
        && std::abs(second_distance) <= tolerance) {
        const Vec2 first_2d = project_2d(first, dropped_axis);
        const Vec2 second_2d = project_2d(second, dropped_axis);
        if (point_in_triangle_2d(
                first_2d,
                triangle_2d,
                tolerance)
            || point_in_triangle_2d(
                second_2d,
                triangle_2d,
                tolerance)) {
            return true;
        }

        for (std::size_t edge = 0; edge < 3; ++edge) {
            if (segments_intersect_2d(
                    first_2d,
                    second_2d,
                    triangle_2d[edge],
                    triangle_2d[(edge + 1) % 3],
                    tolerance)) {
                return true;
            }
        }
        return false;
    }

    const float parameter = first_distance
        / (first_distance - second_distance);
    const Vec3 intersection = add(
        first,
        scale(subtract(second, first), parameter));
    return point_in_triangle_2d(
        project_2d(intersection, dropped_axis),
        triangle_2d,
        tolerance);
}

bool degenerate_intersection(
    const Triangle& left,
    const Triangle& right,
    const Primitive& left_primitive,
    const Primitive& right_primitive,
    const Vec3& left_normal,
    const Vec3& right_normal,
    const float tolerance) noexcept
{
    const float tolerance_squared = tolerance * tolerance;

    if (left_primitive.kind == PrimitiveKind::point
        && right_primitive.kind == PrimitiveKind::point) {
        return length_squared(subtract(
            left_primitive.first,
            right_primitive.first)) <= tolerance_squared;
    }

    if (left_primitive.kind == PrimitiveKind::point
        && right_primitive.kind == PrimitiveKind::segment) {
        return point_segment_distance_squared(
            left_primitive.first,
            right_primitive.first,
            right_primitive.second) <= tolerance_squared;
    }
    if (left_primitive.kind == PrimitiveKind::segment
        && right_primitive.kind == PrimitiveKind::point) {
        return point_segment_distance_squared(
            right_primitive.first,
            left_primitive.first,
            left_primitive.second) <= tolerance_squared;
    }
    if (left_primitive.kind == PrimitiveKind::segment
        && right_primitive.kind == PrimitiveKind::segment) {
        return segment_segment_distance_squared(
            left_primitive.first,
            left_primitive.second,
            right_primitive.first,
            right_primitive.second) <= tolerance_squared;
    }

    if (left_primitive.kind == PrimitiveKind::point) {
        return point_on_triangle(
            left_primitive.first,
            right,
            right_normal,
            tolerance);
    }
    if (right_primitive.kind == PrimitiveKind::point) {
        return point_on_triangle(
            right_primitive.first,
            left,
            left_normal,
            tolerance);
    }
    if (left_primitive.kind == PrimitiveKind::segment) {
        return segment_intersects_triangle(
            left_primitive.first,
            left_primitive.second,
            right,
            right_normal,
            tolerance);
    }
    return segment_intersects_triangle(
        right_primitive.first,
        right_primitive.second,
        left,
        left_normal,
        tolerance);
}

std::array<float, 3> signed_distances(
    const Triangle& triangle,
    const Vec3& plane_point,
    const Vec3& unit_normal) noexcept
{
    return {
        dot(unit_normal, subtract(triangle.a, plane_point)),
        dot(unit_normal, subtract(triangle.b, plane_point)),
        dot(unit_normal, subtract(triangle.c, plane_point)),
    };
}

bool separated_by_plane(
    const std::array<float, 3>& distances,
    const float tolerance) noexcept
{
    return std::all_of(
               distances.begin(),
               distances.end(),
               [tolerance](const float distance) {
                   return distance > tolerance;
               })
        || std::all_of(
            distances.begin(),
            distances.end(),
            [tolerance](const float distance) {
                return distance < -tolerance;
            });
}

Interval line_interval(
    const Triangle& triangle,
    const std::array<float, 3>& distances,
    const Vec3& line_direction,
    const float tolerance) noexcept
{
    const auto points = vertices(triangle);
    float minimum = std::numeric_limits<float>::max();
    float maximum = std::numeric_limits<float>::lowest();

    auto add_projection = [&](const Vec3& point) {
        const float projection = dot(point, line_direction);
        minimum = std::min(minimum, projection);
        maximum = std::max(maximum, projection);
    };

    for (std::size_t index = 0; index < 3; ++index) {
        if (std::abs(distances[index]) <= tolerance) {
            add_projection(points[index]);
        }
    }

    for (std::size_t edge = 0; edge < 3; ++edge) {
        const std::size_t next = (edge + 1) % 3;
        const float first_distance = distances[edge];
        const float second_distance = distances[next];
        if ((first_distance < -tolerance && second_distance > tolerance)
            || (first_distance > tolerance
                && second_distance < -tolerance)) {
            const float parameter = first_distance
                / (first_distance - second_distance);
            add_projection(add(
                points[edge],
                scale(subtract(points[next], points[edge]), parameter)));
        }
    }

    return {minimum, maximum};
}

bool triangles_intersect(
    const Triangle& left,
    const Triangle& right) noexcept
{
    const float tolerance = collision_tolerance(left, right);
    const Vec3 left_normal = cross(
        subtract(left.b, left.a),
        subtract(left.c, left.a));
    const Vec3 right_normal = cross(
        subtract(right.b, right.a),
        subtract(right.c, right.a));

    const Primitive left_primitive = primitive(left, tolerance);
    const Primitive right_primitive = primitive(right, tolerance);
    if (left_primitive.kind != PrimitiveKind::triangle
        || right_primitive.kind != PrimitiveKind::triangle) {
        return degenerate_intersection(
            left,
            right,
            left_primitive,
            right_primitive,
            left_normal,
            right_normal,
            tolerance);
    }

    const Vec3 left_unit_normal = normalize(left_normal);
    const Vec3 right_unit_normal = normalize(right_normal);
    const auto right_to_left_plane = signed_distances(
        right,
        left.a,
        left_unit_normal);
    if (separated_by_plane(right_to_left_plane, tolerance)) {
        return false;
    }

    const auto left_to_right_plane = signed_distances(
        left,
        right.a,
        right_unit_normal);
    if (separated_by_plane(left_to_right_plane, tolerance)) {
        return false;
    }

    const Vec3 line = cross(left_unit_normal, right_unit_normal);
    const float line_length = length(line);
    if (line_length <= 32.0F * std::numeric_limits<float>::epsilon()) {
        const bool coplanar = std::all_of(
            right_to_left_plane.begin(),
            right_to_left_plane.end(),
            [tolerance](const float distance) {
                return std::abs(distance) <= tolerance;
            });
        if (!coplanar) {
            return false;
        }

        return coplanar_triangles_intersect(
            left,
            right,
            left_unit_normal,
            tolerance);
    }

    const Vec3 line_direction = scale(line, 1.0F / line_length);
    const Interval left_interval = line_interval(
        left,
        left_to_right_plane,
        line_direction,
        tolerance);
    const Interval right_interval = line_interval(
        right,
        right_to_left_plane,
        line_direction,
        tolerance);

    return left_interval.minimum <= right_interval.maximum + tolerance
        && right_interval.minimum <= left_interval.maximum + tolerance;
}

}

std::vector<bool> cpu_collisions(const std::vector<Triangle>& triangles)
{
    std::vector<bool> collisions(triangles.size(), false);
    if (triangles.size() < 2) {
        return collisions;
    }

    std::vector<Aabb> bounds;
    bounds.reserve(triangles.size());

    for (const Triangle& triangle : triangles) {
        const Aabb triangle_bounds = aabb(triangle);
        bounds.push_back(triangle_bounds);
    }

    const VoxelGrid voxel_grid = make_voxel_grid(bounds);

    std::vector<IndexedTriangle> indexed_triangles;
    indexed_triangles.reserve(triangles.size());

    std::unordered_map<Voxel, std::vector<std::size_t>, VoxelHash> grid;
    grid.reserve(triangles.size());

    for (std::size_t id = 0; id < triangles.size(); ++id) {
        const Voxel voxel = voxel_for(
            center(bounds[id]),
            voxel_grid);
        indexed_triangles.push_back({id, bounds[id], voxel});
        grid[voxel].push_back(id);
    }

    for (const IndexedTriangle& current : indexed_triangles) {
        for (std::int64_t x_offset = -1; x_offset <= 1; ++x_offset) {
            for (std::int64_t y_offset = -1; y_offset <= 1; ++y_offset) {
                for (std::int64_t z_offset = -1; z_offset <= 1; ++z_offset) {
                    const Voxel neighbor{
                        current.voxel.x + x_offset,
                        current.voxel.y + y_offset,
                        current.voxel.z + z_offset,
                    };
                    const auto bucket = grid.find(neighbor);
                    if (bucket == grid.end()) {
                        continue;
                    }

                    for (const std::size_t candidate_id : bucket->second) {
                        if (candidate_id <= current.id) {
                            continue;
                        }
                        if (collisions[current.id]
                            && collisions[candidate_id]) {
                            continue;
                        }
                        if (!aabbs_intersect(
                                current.bounds,
                                bounds[candidate_id])) {
                            continue;
                        }
                        if (!triangles_intersect(
                                triangles[current.id],
                                triangles[candidate_id])) {
                            continue;
                        }

                        collisions[current.id] = true;
                        collisions[candidate_id] = true;
                    }
                }
            }
        }
    }

    return collisions;
}

}
