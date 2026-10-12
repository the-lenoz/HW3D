module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>

module hw3d.cpu_intersections;

import hw3d.geometry;

namespace hw3d {
namespace {

enum class Projection : std::uint8_t { xy, xz, yz };

struct Point {
    Vec3 position;
};
struct Segment {
    Vec3 first;
    Vec3 second;
};
struct TriangleSurface {
    Triangle vertices;
    Projection projection;
};

using Primitive = std::variant<Point, Segment, TriangleSurface>;

struct BroadPhaseEntry {
    std::size_t id;
    Aabb bounds;
    Voxel voxel;
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

Primitive make_primitive(const Triangle& triangle)
{
    const std::array points{triangle.a, triangle.b, triangle.c};
    if (same_point(points[0], points[1])
        && same_point(points[0], points[2])) {
        return Point{points[0]};
    }

    if (!is_degenerate(triangle)) {
        for (const Projection projection : projections) {
            if (projected_orientation(
                    points[0], points[1], points[2], projection)
                != Sign::zero) {
                return TriangleSurface{triangle, projection};
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
    return Segment{*minimum, *maximum};
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
    const TriangleSurface& triangle)
{
    return orient3d(
            triangle.vertices.a,
            triangle.vertices.b,
            triangle.vertices.c,
            point) == Sign::zero
        && point_in_triangle_2d(
            point,
            triangle.vertices,
            triangle.projection);
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
    const TriangleSurface& triangle)
{
    if (point_in_triangle_2d(
            first, triangle.vertices, triangle.projection)
        || point_in_triangle_2d(
            second, triangle.vertices, triangle.projection)) {
        return true;
    }
    const std::array vertices{
        triangle.vertices.a,
        triangle.vertices.b,
        triangle.vertices.c,
    };
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        if (segments_intersect_2d(
                project(first, triangle.projection),
                project(second, triangle.projection),
                project(vertices[index], triangle.projection),
                project(
                    vertices[(index + 1) % vertices.size()],
                    triangle.projection))) {
            return true;
        }
    }
    return false;
}

bool segment_intersects_triangle(
    const Segment& segment,
    const TriangleSurface& triangle)
{
    const Sign first_side = orient3d(
        triangle.vertices.a,
        triangle.vertices.b,
        triangle.vertices.c,
        segment.first);
    const Sign second_side = orient3d(
        triangle.vertices.a,
        triangle.vertices.b,
        triangle.vertices.c,
        segment.second);
    if (first_side == Sign::zero && second_side == Sign::zero) {
        return coplanar_segment_intersects_triangle(segment.first, segment.second, triangle);
    }
    if (first_side == Sign::zero) {
        return point_intersects_triangle(segment.first, triangle);
    }
    if (second_side == Sign::zero) {
        return point_intersects_triangle(segment.second, triangle);
    }
    if (!opposite(first_side, second_side)) {
        return false;
    }
    return signs_are_consistent(
        orient3d(
            segment.first, segment.second,
            triangle.vertices.a, triangle.vertices.b),
        orient3d(
            segment.first, segment.second,
            triangle.vertices.b, triangle.vertices.c),
        orient3d(
            segment.first, segment.second,
            triangle.vertices.c, triangle.vertices.a));
}

bool intersects(const Point& left, const Point& right) noexcept
{
    return same_point(left.position, right.position);
}

bool intersects(const Point& point, const Segment& segment)
{
    return point_on_segment(point.position, segment.first, segment.second);
}

bool intersects(const Segment& segment, const Point& point)
{
    return intersects(point, segment);
}

bool intersects(const Segment& left, const Segment& right)
{
    return segments_intersect(
        left.first, left.second, right.first, right.second);
}

bool intersects(const Point& point, const TriangleSurface& triangle)
{
    return point_intersects_triangle(point.position, triangle);
}

bool intersects(const TriangleSurface& triangle, const Point& point)
{
    return intersects(point, triangle);
}

bool intersects(const Segment& segment, const TriangleSurface& triangle)
{
    return segment_intersects_triangle(segment, triangle);
}

bool intersects(const TriangleSurface& triangle, const Segment& segment)
{
    return intersects(segment, triangle);
}

bool intersects(
    const TriangleSurface& left,
    const TriangleSurface& right)
{
    const std::array left_vertices{
        left.vertices.a,
        left.vertices.b,
        left.vertices.c,
    };
    const std::array right_vertices{
        right.vertices.a,
        right.vertices.b,
        right.vertices.c,
    };
    for (std::size_t index = 0; index < left_vertices.size(); ++index) {
        if (intersects(
                Segment{
                    left_vertices[index],
                    left_vertices[(index + 1) % left_vertices.size()],
                },
                right)) {
            return true;
        }
    }
    for (std::size_t index = 0; index < right_vertices.size(); ++index) {
        if (intersects(
                Segment{
                    right_vertices[index],
                    right_vertices[(index + 1) % right_vertices.size()],
                },
                left)) {
            return true;
        }
    }
    return false;
}

bool intersects(const Primitive& left, const Primitive& right)
{
    return std::visit(
        [](const auto& first, const auto& second) {
            return intersects(first, second);
        },
        left,
        right);
}

DoubleVoxelGrid make_double_voxel_grid(
    std::span<Aabb> bounds) noexcept
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

void cpu_collisions(std::vector<Triangle>& triangles)
{
    for (const Triangle& triangle : triangles) {
        if (!finite(triangle.a) || !finite(triangle.b) || !finite(triangle.c)) {
            throw std::invalid_argument("triangle coordinates must be finite");
        }
    }
    for (Triangle& triangle : triangles) {
        triangle.highlighted = false;
    }
    if (triangles.size() < 2) {
        return;
    }

    std::vector<Aabb> bounds;
    bounds.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        bounds.push_back(aabb(triangle));
    }

    const DoubleVoxelGrid voxel_grid = make_double_voxel_grid(bounds);
    std::vector<Primitive> primitives;
    primitives.reserve(triangles.size());
    std::vector<BroadPhaseEntry> entries;
    entries.reserve(triangles.size());
    std::unordered_map<Voxel, std::vector<std::size_t>, VoxelHash> grid;
    grid.reserve(triangles.size());

    for (std::size_t id = 0; id < triangles.size(); ++id) {
        const Voxel voxel = double_voxel_for(bounds[id], voxel_grid);
        primitives.push_back(make_primitive(triangles[id]));
        entries.push_back({id, bounds[id], voxel});
        grid[voxel].push_back(id);
    }

    for (const BroadPhaseEntry& current : entries) {
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
                        if (triangles[current.id].highlighted
                            && triangles[candidate_id].highlighted) {
                            continue;
                        }
                        if (!aabbs_intersect(current.bounds, bounds[candidate_id])) {
                            continue;
                        }
                        if (!intersects(
                                primitives[current.id],
                                primitives[candidate_id])) {
                            continue;
                        }
                        triangles[current.id].highlighted = true;
                        triangles[candidate_id].highlighted = true;
                    }
                }
            }
        }
    }
}

}
