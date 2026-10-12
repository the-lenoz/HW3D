module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

export module hw3d.geometry;

import hw3d.configuration;

export namespace hw3d {

enum class Sign : std::int8_t {
    negative = -1,
    zero = 0,
    positive = 1,
};

template<class Scalar>
struct Vector2 {
    Scalar x;
    Scalar y;
};

template<class Scalar>
struct Vector3 {
    Scalar x;
    Scalar y;
    Scalar z;
};

class Expansion final {
public:
    Expansion() = default;
    explicit Expansion(float exact_value);

    [[nodiscard]] bool is_zero() const noexcept;
    [[nodiscard]] Sign sign() const noexcept;

    friend bool operator==(const Expansion&, const Expansion&);
    friend Expansion operator+(const Expansion&, const Expansion&);
    friend Expansion operator-(const Expansion&, const Expansion&);
    friend Expansion operator-(const Expansion&);
    friend Expansion operator*(const Expansion&, const Expansion&);

private:
    explicit Expansion(std::span<const double> components);

    std::vector<double> components_;
};

[[nodiscard]] Sign orient2d(
    const Vector2<float>& a,
    const Vector2<float>& b,
    const Vector2<float>& c);

[[nodiscard]] Sign orient3d(
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const Vec3& d);

[[nodiscard]] bool is_degenerate(const Triangle& triangle);

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

[[nodiscard]] VoxelGrid make_voxel_grid(std::span<const Aabb> bounds) noexcept;
[[nodiscard]] Voxel voxel_for(
    const Vec3& point,
    const VoxelGrid& grid) noexcept;

}
