#include <gtest/gtest.h>

#include <array>
#include <cstdint>

import hw3d.configuration;
import hw3d.geometry;

namespace {

using hw3d::Expansion;
using hw3d::Sign;
using hw3d::Vec3;
using hw3d::Vector2;

Sign sign_of(const __int128 value) noexcept
{
    if (value < 0) return Sign::negative;
    if (value > 0) return Sign::positive;
    return Sign::zero;
}

TEST(Expansion, PreservesAComponentBelowTheLeadingValueUlp)
{
    const Expansion large(1.0e16F);
    const Expansion one(1.0F);
    EXPECT_EQ((large + one) - large, one);
}

TEST(Expansion, CancelsExactlyAndUsesCanonicalZero)
{
    const Expansion value = Expansion(16'777'216.0F) + Expansion(1.0F);
    const Expansion zero = value - value;
    EXPECT_TRUE(zero.is_zero());
    EXPECT_EQ(zero.sign(), Sign::zero);
    EXPECT_EQ(zero, Expansion{});
}

TEST(Expansion, MultipliesExactlyThroughTheFmaResidual)
{
    const Expansion product = Expansion(4097.0F) * Expansion(4097.0F);
    const Expansion expected = Expansion(16'785'408.0F) + Expansion(1.0F);
    EXPECT_EQ(product, expected);
    EXPECT_EQ((-product).sign(), Sign::negative);
}

TEST(RobustPredicates, Orient2dHasExpectedSignsAndPermutation)
{
    constexpr Vector2<float> a{0.0F, 0.0F};
    constexpr Vector2<float> b{2.0F, 0.0F};
    constexpr Vector2<float> c{1.0F, 1.0F};
    EXPECT_EQ(hw3d::orient2d(a, b, c), Sign::positive);
    EXPECT_EQ(hw3d::orient2d(b, a, c), Sign::negative);
    EXPECT_EQ(hw3d::orient2d(a, c, b), Sign::negative);
    EXPECT_EQ(hw3d::orient2d(a, b, {1.0F, 0.0F}), Sign::zero);
}

TEST(RobustPredicates, Orient3dHasExpectedSignsAndPermutation)
{
    constexpr Vec3 a{0.0F, 0.0F, 0.0F};
    constexpr Vec3 b{1.0F, 0.0F, 0.0F};
    constexpr Vec3 c{0.0F, 1.0F, 0.0F};
    constexpr Vec3 d{0.0F, 0.0F, 1.0F};
    const Sign sign = hw3d::orient3d(a, b, c, d);
    EXPECT_NE(sign, Sign::zero);
    EXPECT_EQ(hw3d::orient3d(b, a, c, d),
        sign == Sign::positive ? Sign::negative : Sign::positive);
    EXPECT_EQ(hw3d::orient3d(a, b, c, {0.25F, 0.25F, 0.0F}),
        Sign::zero);
}

TEST(RobustPredicates, ResolvesOrient2dWhenTheDoubleDeterminantCancels)
{
    EXPECT_EQ(hw3d::orient2d(
        {1.0670176527549039e-14F, -6.084565572415083e-23F},
        {6.877835813760436e-36F, -3.972171353979535e-38F},
        {-5.7064873383899955e+17F, -0.00028325230232439935F}),
        Sign::positive);
    EXPECT_EQ(hw3d::orient2d(
        {9.602541897434843e-24F, 3.175886637007963e-11F},
        {1.1873493724681716e-12F, 0.0013336106203496456F},
        {1.1747628185250006e+26F, 2.95879819093693e+30F}),
        Sign::negative);
}

TEST(RobustPredicates, ResolvesOrient3dInsideTheDoubleErrorBound)
{
    EXPECT_EQ(hw3d::orient3d(
        {-2.78783818252748e-09F, 1.318775290889711e-26F,
            17.487215042114258F},
        {2.2518509067767265e-20F, 3.0408849255489546e-31F,
            6.330330197891464e-25F},
        {7.641788622577001e-31F, -4.727059076346284e-37F,
            -0.0005798718193545938F},
        {-1122.851806640625F, 1.4686858347756099e-22F,
            -9.25995979809751e+17F}),
        Sign::negative);
}

TEST(RobustPredicates, MatchesExactIntegerOrient2dOracle)
{
    std::uint32_t state = 0x8a5cd789U;
    const auto next = [&]() {
        state = state * 1'664'525U + 1'013'904'223U;
        return static_cast<std::int64_t>(state % 2'000'001U) - 1'000'000;
    };
    for (int iteration = 0; iteration < 2'000; ++iteration) {
        const std::int64_t ax = next();
        const std::int64_t ay = next();
        const std::int64_t bx = next();
        const std::int64_t by = next();
        const std::int64_t cx = next();
        const std::int64_t cy = next();
        const __int128 determinant =
            static_cast<__int128>(ax - cx) * (by - cy)
            - static_cast<__int128>(ay - cy) * (bx - cx);
        EXPECT_EQ(hw3d::orient2d(
            {static_cast<float>(ax), static_cast<float>(ay)},
            {static_cast<float>(bx), static_cast<float>(by)},
            {static_cast<float>(cx), static_cast<float>(cy)}),
            sign_of(determinant));
    }
}

TEST(RobustPredicates, MatchesExactIntegerOrient3dOracle)
{
    constexpr std::array<std::array<std::int64_t, 12>, 4> cases{{
        {0, 0, 0, 7, 0, 0, 0, 11, 0, 1, 1, 13},
        {10, -2, 3, -4, 8, 1, 7, 5, -6, 2, 3, 4},
        {0, 0, 0, 5, 0, 0, 0, 5, 0, 2, 2, 0},
        {1'000'000, 1, 0, 1, 1'000'000, 0, 0, 0, 1, 2, 3, 4},
    }};
    for (const auto& p : cases) {
        const __int128 adx = p[0] - p[9];
        const __int128 ady = p[1] - p[10];
        const __int128 adz = p[2] - p[11];
        const __int128 bdx = p[3] - p[9];
        const __int128 bdy = p[4] - p[10];
        const __int128 bdz = p[5] - p[11];
        const __int128 cdx = p[6] - p[9];
        const __int128 cdy = p[7] - p[10];
        const __int128 cdz = p[8] - p[11];
        const __int128 determinant = adz * (bdx * cdy - cdx * bdy)
            + bdz * (cdx * ady - adx * cdy)
            + cdz * (adx * bdy - bdx * ady);
        EXPECT_EQ(hw3d::orient3d(
            {static_cast<float>(p[0]), static_cast<float>(p[1]),
                static_cast<float>(p[2])},
            {static_cast<float>(p[3]), static_cast<float>(p[4]),
                static_cast<float>(p[5])},
            {static_cast<float>(p[6]), static_cast<float>(p[7]),
                static_cast<float>(p[8])},
            {static_cast<float>(p[9]), static_cast<float>(p[10]),
                static_cast<float>(p[11])}),
            sign_of(determinant));
    }
}

TEST(RobustPredicates, PreservesSignUnderRepresentableTranslationAndScaling)
{
    constexpr Vector2<float> a{0.0F, 0.0F};
    constexpr Vector2<float> b{2.0F, 0.0F};
    constexpr Vector2<float> c{1.0F, 0x1p-8F};
    const Sign original = hw3d::orient2d(a, b, c);
    EXPECT_EQ(original, Sign::positive);
    EXPECT_EQ(hw3d::orient2d(
        {a.x * 0x1p-30F, a.y * 0x1p-30F},
        {b.x * 0x1p-30F, b.y * 0x1p-30F},
        {c.x * 0x1p-30F, c.y * 0x1p-30F}), original);
    EXPECT_EQ(hw3d::orient2d(
        {a.x + 1024.0F, a.y + 1024.0F},
        {b.x + 1024.0F, b.y + 1024.0F},
        {c.x + 1024.0F, c.y + 1024.0F}), original);
}

}
