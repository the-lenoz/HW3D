module;

#include <vector>

export module hw3d.cpu_intersections;

import hw3d.configuration;

export namespace hw3d {

[[nodiscard]] std::vector<bool> cpu_collisions(
    const std::vector<Triangle>& triangles);

}
