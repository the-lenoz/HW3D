module;

#include <vector>

export module hw3d.gpu_intersections;
import hw3d.configuration;

export namespace hw3d {
void gpu_collisions(std::vector<Triangle>& triangles);
}
