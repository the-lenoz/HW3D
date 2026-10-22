module;

#include <memory>
#include <cstddef>
#include <span>
#include <vector>

export module hw3d.cpu_intersections;

import hw3d.geometry;

export namespace hw3d {

class CpuIntersectionDetector final {
public:
    CpuIntersectionDetector(
        std::vector<Triangle>& triangles,
        std::span<const Aabb> motion_bounds,
        std::size_t maximum_pair_count =
            maximum_cached_triangle_pair_count);
    ~CpuIntersectionDetector();

    CpuIntersectionDetector(const CpuIntersectionDetector&) = delete;
    CpuIntersectionDetector& operator=(const CpuIntersectionDetector&) = delete;
    CpuIntersectionDetector(CpuIntersectionDetector&&) = delete;
    CpuIntersectionDetector& operator=(CpuIntersectionDetector&&) = delete;

    void update();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
