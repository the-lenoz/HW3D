module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

module hw3d.cpu_intersections;

import hw3d.geometry;

namespace hw3d {
namespace {

enum class PrimitiveKind : std::uint8_t { point, segment, triangle };
enum class Projection : std::uint8_t { xy, xz, yz };

struct Primitive {
    PrimitiveKind kind;
    std::uint8_t first_index;
    std::uint8_t second_index;
    Projection projection;
};

struct IndexedTriangle {
    std::size_t id;
    Aabb bounds;
    Voxel voxel;
    Primitive primitive;
};

struct DVec3 { double x; double y; double z; };
struct DoubleVoxelGrid { DVec3 origin; DVec3 voxel_size; };

constexpr std::array<Projection, 3> projections{
    Projection::xy, Projection::xz, Projection::yz,
};

bool same_point(const Vec3& left, const Vec3& right) noexcept
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

const Vec3& vertex(const Triangle& triangle, const std::size_t index) noexcept
{
    if (index == 0) return triangle.a;
    if (index == 1) return triangle.b;
    return triangle.c;
}

Vector2<float> project(const Vec3& point, const Projection projection) noexcept
{
    switch (projection) {
    case Projection::xy: return {point.x, point.y};
    case Projection::xz: return {point.x, point.z};
    case Projection::yz: return {point.y, point.z};
    }
    return {};
}

Sign projected_orientation(
    const Vec3& a, const Vec3& b, const Vec3& c,
    const Projection projection)
{
    return orient2d(
        project(a, projection), project(b, projection), project(c, projection));
}

Primitive classify(const Triangle& triangle)
{
    const std::array points{triangle.a, triangle.b, triangle.c};
    if (same_point(points[0], points[1])
        && same_point(points[0], points[2])) {
        return {PrimitiveKind::point, 0, 0, Projection::xy};
    }

    if (!is_degenerate(triangle)) {
        for (const Projection projection : projections) {
            if (projected_orientation(
                    points[0], points[1], points[2], projection)
                != Sign::zero) {
                return {PrimitiveKind::triangle, 0, 0, projection};
            }
        }
    }

    const auto coordinate = [&](const Vec3& point) noexcept {
        if (points[0].x != points[1].x || points[0].x != points[2].x) {
            return point.x;
        }
        if (points[0].y != points[1].y || points[0].y != points[2].y) {
            return point.y;
        }
        return point.z;
    };
    const auto minimum = std::min_element(
        points.begin(), points.end(), [&](const Vec3& left, const Vec3& right) {
            return coordinate(left) < coordinate(right);
        });
    const auto maximum = std::max_element(
        points.begin(), points.end(), [&](const Vec3& left, const Vec3& right) {
            return coordinate(left) < coordinate(right);
        });
    return {
        PrimitiveKind::segment,
        static_cast<std::uint8_t>(minimum - points.begin()),
        static_cast<std::uint8_t>(maximum - points.begin()),
        Projection::xy,
    };
}

bool between(const float value, const float first, const float second) noexcept
{
    return value >= std::min(first, second) && value <= std::max(first, second);
}

bool point_in_box(
    const Vec3& point, const Vec3& first, const Vec3& second) noexcept
{
    return between(point.x, first.x, second.x)
        && between(point.y, first.y, second.y)
        && between(point.z, first.z, second.z);
}

bool point_on_segment(
    const Vec3& point, const Vec3& first, const Vec3& second)
{
    for (const Projection projection : projections) {
        if (projected_orientation(first, second, point, projection)
            != Sign::zero) {
            return false;
        }
    }
    return point_in_box(point, first, second);
}

bool point_on_segment_2d(
    const Vector2<float>& point,
    const Vector2<float>& first,
    const Vector2<float>& second) noexcept
{
    return between(point.x, first.x, second.x)
        && between(point.y, first.y, second.y);
}

bool opposite(const Sign left, const Sign right) noexcept
{
    return (left == Sign::negative && right == Sign::positive)
        || (left == Sign::positive && right == Sign::negative);
}

bool segments_intersect_2d(
    const Vector2<float>& a, const Vector2<float>& b,
    const Vector2<float>& c, const Vector2<float>& d)
{
    const Sign abc = orient2d(a, b, c);
    const Sign abd = orient2d(a, b, d);
    const Sign cda = orient2d(c, d, a);
    const Sign cdb = orient2d(c, d, b);
    if (opposite(abc, abd) && opposite(cda, cdb)) {
        return true;
    }
    return (abc == Sign::zero && point_on_segment_2d(c, a, b))
        || (abd == Sign::zero && point_on_segment_2d(d, a, b))
        || (cda == Sign::zero && point_on_segment_2d(a, c, d))
        || (cdb == Sign::zero && point_on_segment_2d(b, c, d));
}

bool point_in_triangle_2d(
    const Vec3& point,
    const Triangle& triangle,
    const Projection projection)
{
    const Vector2<float> p = project(point, projection);
    const Sign first = orient2d(
        project(triangle.a, projection), project(triangle.b, projection), p);
    const Sign second = orient2d(
        project(triangle.b, projection), project(triangle.c, projection), p);
    const Sign third = orient2d(
        project(triangle.c, projection), project(triangle.a, projection), p);
    const bool has_negative = first == Sign::negative
        || second == Sign::negative || third == Sign::negative;
    const bool has_positive = first == Sign::positive
        || second == Sign::positive || third == Sign::positive;
    return !(has_negative && has_positive);
}

bool point_intersects_triangle(
    const Vec3& point,
    const Triangle& triangle,
    const Primitive& primitive)
{
    return orient3d(triangle.a, triangle.b, triangle.c, point) == Sign::zero
        && point_in_triangle_2d(point, triangle, primitive.projection);
}

bool boxes_overlap(
    const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d) noexcept
{
    return std::max(std::min(a.x, b.x), std::min(c.x, d.x))
            <= std::min(std::max(a.x, b.x), std::max(c.x, d.x))
        && std::max(std::min(a.y, b.y), std::min(c.y, d.y))
            <= std::min(std::max(a.y, b.y), std::max(c.y, d.y))
        && std::max(std::min(a.z, b.z), std::min(c.z, d.z))
            <= std::min(std::max(a.z, b.z), std::max(c.z, d.z));
}

bool segments_intersect(
    const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
{
    if (orient3d(a, b, c, d) != Sign::zero) {
        return false;
    }
    for (const Projection projection : projections) {
        if (projected_orientation(a, b, c, projection) != Sign::zero
            || projected_orientation(a, b, d, projection) != Sign::zero) {
            return segments_intersect_2d(
                project(a, projection), project(b, projection),
                project(c, projection), project(d, projection));
        }
    }
    return boxes_overlap(a, b, c, d);
}

bool signs_are_consistent(
    const Sign first, const Sign second, const Sign third) noexcept
{
    const bool has_negative = first == Sign::negative
        || second == Sign::negative || third == Sign::negative;
    const bool has_positive = first == Sign::positive
        || second == Sign::positive || third == Sign::positive;
    return !(has_negative && has_positive);
}

bool coplanar_segment_intersects_triangle(
    const Vec3& first,
    const Vec3& second,
    const Triangle& triangle,
    const Projection projection)
{
    if (point_in_triangle_2d(first, triangle, projection)
        || point_in_triangle_2d(second, triangle, projection)) {
        return true;
    }
    const std::array vertices{triangle.a, triangle.b, triangle.c};
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        if (segments_intersect_2d(
                project(first, projection), project(second, projection),
                project(vertices[index], projection),
                project(vertices[(index + 1) % vertices.size()], projection))) {
            return true;
        }
    }
    return false;
}

bool segment_intersects_triangle(
    const Vec3& first,
    const Vec3& second,
    const Triangle& triangle,
    const Primitive& primitive)
{
    const Sign first_side = orient3d(
        triangle.a, triangle.b, triangle.c, first);
    const Sign second_side = orient3d(
        triangle.a, triangle.b, triangle.c, second);
    if (first_side == Sign::zero && second_side == Sign::zero) {
        return coplanar_segment_intersects_triangle(
            first, second, triangle, primitive.projection);
    }
    if (first_side == Sign::zero) {
        return point_intersects_triangle(first, triangle, primitive);
    }
    if (second_side == Sign::zero) {
        return point_intersects_triangle(second, triangle, primitive);
    }
    if (!opposite(first_side, second_side)) {
        return false;
    }
    return signs_are_consistent(
        orient3d(first, second, triangle.a, triangle.b),
        orient3d(first, second, triangle.b, triangle.c),
        orient3d(first, second, triangle.c, triangle.a));
}

bool triangles_intersect(
    const Triangle& left,
    const Triangle& right,
    const Primitive& left_primitive,
    const Primitive& right_primitive)
{
    const Vec3& left_first = vertex(left, left_primitive.first_index);
    const Vec3& left_second = vertex(left, left_primitive.second_index);
    const Vec3& right_first = vertex(right, right_primitive.first_index);
    const Vec3& right_second = vertex(right, right_primitive.second_index);

    if (left_primitive.kind == PrimitiveKind::point
        && right_primitive.kind == PrimitiveKind::point) {
        return same_point(left_first, right_first);
    }
    if (left_primitive.kind == PrimitiveKind::point
        && right_primitive.kind == PrimitiveKind::segment) {
        return point_on_segment(left_first, right_first, right_second);
    }
    if (left_primitive.kind == PrimitiveKind::segment
        && right_primitive.kind == PrimitiveKind::point) {
        return point_on_segment(right_first, left_first, left_second);
    }
    if (left_primitive.kind == PrimitiveKind::segment
        && right_primitive.kind == PrimitiveKind::segment) {
        return segments_intersect(
            left_first, left_second, right_first, right_second);
    }
    if (left_primitive.kind == PrimitiveKind::point) {
        return point_intersects_triangle(left_first, right, right_primitive);
    }
    if (right_primitive.kind == PrimitiveKind::point) {
        return point_intersects_triangle(right_first, left, left_primitive);
    }
    if (left_primitive.kind == PrimitiveKind::segment) {
        return segment_intersects_triangle(
            left_first, left_second, right, right_primitive);
    }
    if (right_primitive.kind == PrimitiveKind::segment) {
        return segment_intersects_triangle(
            right_first, right_second, left, left_primitive);
    }

    const std::array left_vertices{left.a, left.b, left.c};
    const std::array right_vertices{right.a, right.b, right.c};
    for (std::size_t index = 0; index < left_vertices.size(); ++index) {
        if (segment_intersects_triangle(
                left_vertices[index],
                left_vertices[(index + 1) % left_vertices.size()],
                right, right_primitive)) {
            return true;
        }
    }
    for (std::size_t index = 0; index < right_vertices.size(); ++index) {
        if (segment_intersects_triangle(
                right_vertices[index],
                right_vertices[(index + 1) % right_vertices.size()],
                left, left_primitive)) {
            return true;
        }
    }
    return false;
}

DoubleVoxelGrid make_double_voxel_grid(
    const std::vector<Aabb>& bounds) noexcept
{
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
        voxel_size.x = std::max(voxel_size.x,
            static_cast<double>(value.maximum.x) - value.minimum.x);
        voxel_size.y = std::max(voxel_size.y,
            static_cast<double>(value.maximum.y) - value.minimum.y);
        voxel_size.z = std::max(voxel_size.z,
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
    const DVec3 center{
        static_cast<double>(bounds.minimum.x)
            + (static_cast<double>(bounds.maximum.x) - bounds.minimum.x) * 0.5,
        static_cast<double>(bounds.minimum.y)
            + (static_cast<double>(bounds.maximum.y) - bounds.minimum.y) * 0.5,
        static_cast<double>(bounds.minimum.z)
            + (static_cast<double>(bounds.maximum.z) - bounds.minimum.z) * 0.5,
    };
    return {
        double_voxel_coordinate(center.x, grid.origin.x, grid.voxel_size.x),
        double_voxel_coordinate(center.y, grid.origin.y, grid.voxel_size.y),
        double_voxel_coordinate(center.z, grid.origin.z, grid.voxel_size.z),
    };
}

bool finite(const Vec3& point) noexcept
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z);
}

}

std::vector<bool> cpu_collisions(const std::vector<Triangle>& triangles)
{
    for (const Triangle& triangle : triangles) {
        if (!finite(triangle.a) || !finite(triangle.b) || !finite(triangle.c)) {
            throw std::invalid_argument("triangle coordinates must be finite");
        }
    }

    std::vector collisions(triangles.size(), false);
    if (triangles.size() < 2) {
        return collisions;
    }

    std::vector<Aabb> bounds;
    bounds.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        bounds.push_back(aabb(triangle));
    }

    const DoubleVoxelGrid voxel_grid = make_double_voxel_grid(bounds);
    std::vector<IndexedTriangle> indexed_triangles;
    indexed_triangles.reserve(triangles.size());
    std::unordered_map<Voxel, std::vector<std::size_t>, VoxelHash> grid;
    grid.reserve(triangles.size());

    for (std::size_t id = 0; id < triangles.size(); ++id) {
        const Voxel voxel = double_voxel_for(bounds[id], voxel_grid);
        indexed_triangles.push_back({
            id, bounds[id], voxel, classify(triangles[id]),
        });
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
                    if (bucket == grid.end()) continue;
                    for (const std::size_t candidate_id : bucket->second) {
                        if (candidate_id <= current.id) continue;
                        if (collisions[current.id] && collisions[candidate_id]) {
                            continue;
                        }
                        if (!aabbs_intersect(current.bounds, bounds[candidate_id])) {
                            continue;
                        }
                        if (!triangles_intersect(
                                triangles[current.id], triangles[candidate_id],
                                current.primitive,
                                indexed_triangles[candidate_id].primitive)) {
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
