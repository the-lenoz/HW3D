module;

#include <cmath>
#include <limits>

module hw3d.geometry;

namespace hw3d {
namespace {

constexpr double epsilon = std::numeric_limits<double>::epsilon() * 0.5;
constexpr double orient2d_error_bound = (3.0 + 16.0 * epsilon) * epsilon;
constexpr double orient3d_error_bound = (7.0 + 56.0 * epsilon) * epsilon;

Sign sign_of(const double value) noexcept
{
    if (value > 0.0) {
        return Sign::positive;
    }
    if (value < 0.0) {
        return Sign::negative;
    }
    return Sign::zero;
}

Expansion exact_difference(const float left, const float right)
{
    return Expansion(left) - Expansion(right);
}

}

Sign orient2d(
    const Vector2<float>& a,
    const Vector2<float>& b,
    const Vector2<float>& c)
{
    const double acx = static_cast<double>(a.x) - c.x;
    const double bcx = static_cast<double>(b.x) - c.x;
    const double acy = static_cast<double>(a.y) - c.y;
    const double bcy = static_cast<double>(b.y) - c.y;
    const double left = acx * bcy;
    const double right = acy * bcx;
    const double determinant = left - right;
    const double permanent = std::abs(left) + std::abs(right);
    const double error = orient2d_error_bound * permanent;

    if (determinant > error || determinant < -error) {
        return sign_of(determinant);
    }

    const Expansion exact = exact_difference(a.x, c.x)
            * exact_difference(b.y, c.y)
        - exact_difference(a.y, c.y)
            * exact_difference(b.x, c.x);
    return exact.sign();
}

Sign orient3d(
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const Vec3& d)
{
    const double adx = static_cast<double>(a.x) - d.x;
    const double bdx = static_cast<double>(b.x) - d.x;
    const double cdx = static_cast<double>(c.x) - d.x;
    const double ady = static_cast<double>(a.y) - d.y;
    const double bdy = static_cast<double>(b.y) - d.y;
    const double cdy = static_cast<double>(c.y) - d.y;
    const double adz = static_cast<double>(a.z) - d.z;
    const double bdz = static_cast<double>(b.z) - d.z;
    const double cdz = static_cast<double>(c.z) - d.z;

    const double bdxcdy = bdx * cdy;
    const double cdxbdy = cdx * bdy;
    const double cdxady = cdx * ady;
    const double adxcdy = adx * cdy;
    const double adxbdy = adx * bdy;
    const double bdxady = bdx * ady;
    const double determinant = adz * (bdxcdy - cdxbdy)
        + bdz * (cdxady - adxcdy)
        + cdz * (adxbdy - bdxady);
    const double permanent =
        (std::abs(bdxcdy) + std::abs(cdxbdy)) * std::abs(adz)
        + (std::abs(cdxady) + std::abs(adxcdy)) * std::abs(bdz)
        + (std::abs(adxbdy) + std::abs(bdxady)) * std::abs(cdz);
    const double error = orient3d_error_bound * permanent;

    if (determinant > error || determinant < -error) {
        return sign_of(determinant);
    }

    const Expansion exact_adx = exact_difference(a.x, d.x);
    const Expansion exact_bdx = exact_difference(b.x, d.x);
    const Expansion exact_cdx = exact_difference(c.x, d.x);
    const Expansion exact_ady = exact_difference(a.y, d.y);
    const Expansion exact_bdy = exact_difference(b.y, d.y);
    const Expansion exact_cdy = exact_difference(c.y, d.y);
    const Expansion exact_adz = exact_difference(a.z, d.z);
    const Expansion exact_bdz = exact_difference(b.z, d.z);
    const Expansion exact_cdz = exact_difference(c.z, d.z);

    const Expansion exact = exact_adz
            * (exact_bdx * exact_cdy - exact_cdx * exact_bdy)
        + exact_bdz
            * (exact_cdx * exact_ady - exact_adx * exact_cdy)
        + exact_cdz
            * (exact_adx * exact_bdy - exact_bdx * exact_ady);
    return exact.sign();
}

}
