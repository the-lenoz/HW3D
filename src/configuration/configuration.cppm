module;

#include <iosfwd>
#include <chrono>
#include <vector>

export module hw3d.configuration;

export namespace hw3d {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct InitialTriangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    Vec3 rotation_axis_origin{};
    Vec3 rotation_axis_direction{0.0F, 0.0F, 1.0F};
    float angular_velocity_radians_per_second = 0.0F;
};

struct Configuration {
    std::chrono::seconds simulation_duration{};
    std::vector<InitialTriangle> triangles;
};

[[nodiscard]] Configuration read_configuration(std::istream& input);

}
