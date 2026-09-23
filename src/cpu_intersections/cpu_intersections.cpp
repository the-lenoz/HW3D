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

std::array<Vec3, 3> vertices(const Triangle& triangle) noexcept
{
    return {triangle.a, triangle.b, triangle.c};
}

std::array<Vec3, 3> edges(const Triangle& triangle) noexcept
{
    return {
        subtract(triangle.b, triangle.a),
        subtract(triangle.c, triangle.a),
        subtract(triangle.c, triangle.b),
    };
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

Primitive primitive(const Triangle& triangle, const float tolerance) noexcept
{
    const std::array<Vec3, 3> points = vertices(triangle);
    const std::array<std::array<std::size_t, 2>, 3> vertex_pairs{{
        {0, 1},
        {1, 2},
        {2, 0},
    }};

    float longest_squared{};
    std::array<std::size_t, 2> longest_edge{0, 0};
    for (const auto pair : vertex_pairs) {
        const float squared = length_squared(
            subtract(points[pair[1]], points[pair[0]]));
        if (squared > longest_squared) {
            longest_squared = squared;
            longest_edge = pair;
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

bool projections_overlap(
    const Triangle& left,
    const Triangle& right,
    const Vec3& axis,
    const float tolerance) noexcept
{
    const float axis_length = length(axis);
    if (axis_length == 0.0F) {
        return true;
    }

    const auto left_vertices = vertices(left);
    const auto right_vertices = vertices(right);
    float left_minimum = dot(left_vertices[0], axis);
    float left_maximum = left_minimum;
    float right_minimum = dot(right_vertices[0], axis);
    float right_maximum = right_minimum;

    for (std::size_t index = 1; index < 3; ++index) {
        const float left_projection = dot(left_vertices[index], axis);
        left_minimum = std::min(left_minimum, left_projection);
        left_maximum = std::max(left_maximum, left_projection);

        const float right_projection = dot(right_vertices[index], axis);
        right_minimum = std::min(right_minimum, right_projection);
        right_maximum = std::max(right_maximum, right_projection);
    }

    const float projected_tolerance = tolerance * axis_length;
    return left_minimum <= right_maximum + projected_tolerance
        && right_minimum <= left_maximum + projected_tolerance;
}

bool sat_intersection(
    const Triangle& left,
    const Triangle& right,
    const float tolerance) noexcept
{
    const auto left_edges = edges(left);
    const auto right_edges = edges(right);
    const Vec3 left_normal = cross(left_edges[0], left_edges[1]);
    const Vec3 right_normal = cross(right_edges[0], right_edges[1]);

    if (!projections_overlap(left, right, left_normal, tolerance)
        || !projections_overlap(left, right, right_normal, tolerance)) {
        return false;
    }

    for (const Vec3& left_edge : left_edges) {
        for (const Vec3& right_edge : right_edges) {
            if (!projections_overlap(
                    left,
                    right,
                    cross(left_edge, right_edge),
                    tolerance)) {
                return false;
            }
        }
    }

    for (const Vec3& edge : left_edges) {
        if (!projections_overlap(
                left,
                right,
                cross(left_normal, edge),
                tolerance)) {
            return false;
        }
    }
    for (const Vec3& edge : right_edges) {
        if (!projections_overlap(
                left,
                right,
                cross(right_normal, edge),
                tolerance)) {
            return false;
        }
    }

    return true;
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
    return length_squared(subtract(
        point,
        add(first, scale(segment, parameter))));
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

float point_triangle_distance_squared(
    const Vec3& point,
    const Triangle& triangle) noexcept
{
    const Vec3 ab = subtract(triangle.b, triangle.a);
    const Vec3 ac = subtract(triangle.c, triangle.a);
    const Vec3 ap = subtract(point, triangle.a);
    const float d1 = dot(ab, ap);
    const float d2 = dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) {
        return length_squared(ap);
    }

    const Vec3 bp = subtract(point, triangle.b);
    const float d3 = dot(ab, bp);
    const float d4 = dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) {
        return length_squared(bp);
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        const float parameter = d1 / (d1 - d3);
        return length_squared(subtract(
            point,
            add(triangle.a, scale(ab, parameter))));
    }

    const Vec3 cp = subtract(point, triangle.c);
    const float d5 = dot(ab, cp);
    const float d6 = dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) {
        return length_squared(cp);
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        const float parameter = d2 / (d2 - d6);
        return length_squared(subtract(
            point,
            add(triangle.a, scale(ac, parameter))));
    }

    const float va = d3 * d6 - d5 * d4;
    const float d43 = d4 - d3;
    const float d56 = d5 - d6;
    if (va <= 0.0F && d43 >= 0.0F && d56 >= 0.0F) {
        const float parameter = d43 / (d43 + d56);
        return length_squared(subtract(
            point,
            add(triangle.b, scale(
                subtract(triangle.c, triangle.b),
                parameter))));
    }

    const float denominator = 1.0F / (va + vb + vc);
    const float v = vb * denominator;
    const float w = vc * denominator;
    const Vec3 closest = add(
        triangle.a,
        add(scale(ab, v), scale(ac, w)));
    return length_squared(subtract(point, closest));
}

bool segment_intersects_triangle(
    const Vec3& first,
    const Vec3& second,
    const Triangle& triangle,
    const float tolerance) noexcept
{
    const float tolerance_squared = tolerance * tolerance;
    const Vec3 normal = cross(
        subtract(triangle.b, triangle.a),
        subtract(triangle.c, triangle.a));
    const float first_distance = dot(
        normal,
        subtract(first, triangle.a));
    const float second_distance = dot(
        normal,
        subtract(second, triangle.a));

    if (first_distance != second_distance) {
        const float parameter = first_distance
            / (first_distance - second_distance);
        if (parameter >= 0.0F && parameter <= 1.0F) {
            const Vec3 intersection = add(
                first,
                scale(subtract(second, first), parameter));
            if (point_triangle_distance_squared(intersection, triangle)
                <= tolerance_squared) {
                return true;
            }
        }
    }

    if (point_triangle_distance_squared(first, triangle)
            <= tolerance_squared
        || point_triangle_distance_squared(second, triangle)
            <= tolerance_squared) {
        return true;
    }

    const auto triangle_vertices = vertices(triangle);
    for (std::size_t index = 0; index < 3; ++index) {
        if (segment_segment_distance_squared(
                first,
                second,
                triangle_vertices[index],
                triangle_vertices[(index + 1) % 3])
            <= tolerance_squared) {
            return true;
        }
    }

    return false;
}

bool degenerate_intersection(
    const Triangle& left,
    const Triangle& right,
    const Primitive& left_primitive,
    const Primitive& right_primitive,
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
        return point_triangle_distance_squared(
            left_primitive.first,
            right) <= tolerance_squared;
    }
    if (right_primitive.kind == PrimitiveKind::point) {
        return point_triangle_distance_squared(
            right_primitive.first,
            left) <= tolerance_squared;
    }
    if (left_primitive.kind == PrimitiveKind::segment) {
        return segment_intersects_triangle(
            left_primitive.first,
            left_primitive.second,
            right,
            tolerance);
    }
    return segment_intersects_triangle(
        right_primitive.first,
        right_primitive.second,
        left,
        tolerance);
}

bool triangles_intersect(
    const Triangle& left,
    const Triangle& right) noexcept
{
    const float tolerance = collision_tolerance(left, right);
    const Primitive left_primitive = primitive(left, tolerance);
    const Primitive right_primitive = primitive(right, tolerance);

    if (left_primitive.kind == PrimitiveKind::triangle
        && right_primitive.kind == PrimitiveKind::triangle) {
        return sat_intersection(left, right, tolerance);
    }

    return degenerate_intersection(
        left,
        right,
        left_primitive,
        right_primitive,
        tolerance);
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
