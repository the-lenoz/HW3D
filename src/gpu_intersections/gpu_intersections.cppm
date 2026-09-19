module;

#include <vector>

export module hw3d.gpu_intersections;
import hw3d.configuration;

export namespace hw3d {
    [[nodiscard]] std::vector<bool> gpu_collisions(const std::vector<Triangle>& triangles);
}