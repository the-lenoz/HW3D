module;

#include <iosfwd>
#include <vector>

export module hw3d.configuration;

export namespace hw3d {

struct Vec3 {
    double x;
    double y;
    double z;
};

struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

struct Configuration {
    std::vector<Triangle> triangles;
};

[[nodiscard]] Configuration read_configuration(std::istream& input);

}

