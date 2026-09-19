module;

#include <cstddef>
#include <cstdint>
#include <vector>

export module hw3d.geometry;

import hw3d.configuration;

export namespace hw3d {

struct Aabb {
    Vec3 minimum;
    Vec3 maximum;
};

struct Voxel {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;

    bool operator==(const Voxel&) const = default;
};

struct VoxelHash {
    [[nodiscard]] std::size_t operator()(const Voxel& voxel) const noexcept;
};

struct VoxelGrid {
    Vec3 origin;
    Vec3 voxel_size;
};

[[nodiscard]] Aabb aabb(const Triangle& triangle) noexcept;
[[nodiscard]] Vec3 extent(const Aabb& bounds) noexcept;
[[nodiscard]] Vec3 center(const Aabb& bounds) noexcept;
[[nodiscard]] bool aabbs_intersect(
    const Aabb& left,
    const Aabb& right) noexcept;

[[nodiscard]] VoxelGrid make_voxel_grid(
    const std::vector<Aabb>& bounds) noexcept;
[[nodiscard]] Voxel voxel_for(
    const Vec3& point,
    const VoxelGrid& grid) noexcept;

}
