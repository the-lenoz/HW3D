module;

#include <memory>
#include <cstddef>
#include <span>

export module hw3d.gpu_intersections;
import hw3d.geometry;
import hw3d.gpu_triangles;

export namespace hw3d {

class GpuIntersectionDetector final {
public:
    GpuIntersectionDetector(
        GpuTriangleBuffer& triangles,
        std::span<const Aabb> motion_bounds,
        std::size_t maximum_pair_count =
            maximum_cached_triangle_pair_count);
    ~GpuIntersectionDetector();

    GpuIntersectionDetector(const GpuIntersectionDetector&) = delete;
    GpuIntersectionDetector& operator=(const GpuIntersectionDetector&) = delete;
    GpuIntersectionDetector(GpuIntersectionDetector&&) = delete;
    GpuIntersectionDetector& operator=(GpuIntersectionDetector&&) = delete;

    void update();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
