module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

export module hw3d.geometry;

import hw3d.configuration;

export namespace hw3d {

struct Triangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    bool highlighted = false;
};

void update_vertices(
    std::span<const InitialTriangle> initial,
    std::span<Triangle> current,
    std::chrono::duration<double> elapsed);

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

struct TrianglePair {
    std::uint32_t first;
    std::uint32_t second;

    bool operator==(const TrianglePair&) const = default;
};

inline constexpr std::size_t maximum_cached_triangle_pair_count =
    256'000'000;

struct MotionPairCandidates {
    std::vector<Voxel> voxels;
    std::optional<std::vector<TrianglePair>> pairs;
};

[[nodiscard]] Aabb aabb(const Triangle& triangle) noexcept;
[[nodiscard]] Aabb motion_aabb(const InitialTriangle& triangle) noexcept;
void calculate_motion_bounds(
    std::span<const InitialTriangle> initial,
    std::span<Aabb> output);
[[nodiscard]] Vec3 extent(const Aabb& bounds) noexcept;
[[nodiscard]] Vec3 center(const Aabb& bounds) noexcept;

[[nodiscard]] inline bool aabbs_intersect( /* move to cppm to allow cross-module inlining */
    const Aabb& left,
    const Aabb& right) noexcept
{
    return left.minimum.x <= right.maximum.x
        && left.maximum.x >= right.minimum.x
        && left.minimum.y <= right.maximum.y
        && left.maximum.y >= right.minimum.y
        && left.minimum.z <= right.maximum.z
        && left.maximum.z >= right.minimum.z;
}

[[nodiscard]] VoxelGrid make_voxel_grid(std::span<const Aabb> bounds) noexcept;
[[nodiscard]] Voxel voxel_for(
    const Vec3& point,
    const VoxelGrid& grid) noexcept;
[[nodiscard]] MotionPairCandidates make_motion_pair_candidates(
    std::span<const Aabb> motion_bounds,
    std::size_t maximum_pair_count);

}
