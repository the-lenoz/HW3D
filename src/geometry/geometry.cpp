module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
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

bool aabbs_intersect(const Aabb& left, const Aabb& right) noexcept
{
    return left.minimum.x <= right.maximum.x
        && left.maximum.x >= right.minimum.x
        && left.minimum.y <= right.maximum.y
        && left.maximum.y >= right.minimum.y
        && left.minimum.z <= right.maximum.z
        && left.maximum.z >= right.minimum.z;
}

VoxelGrid make_voxel_grid(const std::vector<Aabb>& bounds) noexcept
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

}
