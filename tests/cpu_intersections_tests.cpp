#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>

import hw3d.configuration;
import hw3d.cpu_intersections;

namespace {

using hw3d::Triangle;
using hw3d::Vec3;

Triangle triangle(
    const Vec3 a,
    const Vec3 b,
    const Vec3 c) noexcept
{
    return {a, b, c};
}

Triangle point(const Vec3 value) noexcept
{
    return {value, value, value};
}

Triangle segment(const Vec3 first, const Vec3 second) noexcept
{
    return {first, second, first};
}

Triangle translated(
    const Triangle& value,
    const Vec3 offset) noexcept
{
    const auto translate = [&](const Vec3 point) noexcept {
        return Vec3{
            point.x + offset.x,
            point.y + offset.y,
            point.z + offset.z,
        };
    };
    return {
        translate(value.a),
        translate(value.b),
        translate(value.c),
    };
}

Triangle scaled(const Triangle& value, const float factor) noexcept
{
    const auto scale = [&](const Vec3 point) noexcept {
        return Vec3{
            point.x * factor,
            point.y * factor,
            point.z * factor,
        };
    };
    return {scale(value.a), scale(value.b), scale(value.c)};
}

void expect_collisions(
    const std::vector<Triangle>& triangles,
    const std::initializer_list<bool> expected)
{
    const std::vector<bool> actual = hw3d::cpu_collisions(triangles);

    ASSERT_EQ(actual.size(), expected.size());
    std::size_t index = 0;
    for (const bool value : expected) {
        EXPECT_EQ(actual[index], value) << "triangle index: " << index;
        ++index;
    }
}

const Triangle base_triangle = triangle(
    {0.0F, 0.0F, 0.0F},
    {2.0F, 0.0F, 0.0F},
    {0.0F, 2.0F, 0.0F});

TEST(CpuIntersections, ReturnsEmptyResultForEmptyInput)
{
    expect_collisions({}, {});
}

TEST(CpuIntersections, SingleTriangleDoesNotCollide)
{
    expect_collisions({base_triangle}, {false});
}

TEST(CpuIntersections, DetectsIdenticalTriangles)
{
    expect_collisions({base_triangle, base_triangle}, {true, true});
}

TEST(CpuIntersections, DetectsCoplanarContainment)
{
    const Triangle inner = triangle(
        {0.25F, 0.25F, 0.0F},
        {0.75F, 0.25F, 0.0F},
        {0.25F, 0.75F, 0.0F});

    expect_collisions({base_triangle, inner}, {true, true});
}

TEST(CpuIntersections, RejectsCoplanarTrianglesWithOverlappingAabbs)
{
    const Triangle outside = triangle(
        {1.25F, 1.25F, 0.0F},
        {3.0F, 1.25F, 0.0F},
        {1.25F, 3.0F, 0.0F});

    expect_collisions({base_triangle, outside}, {false, false});
}

TEST(CpuIntersections, CountsSharedEdgeAsCollision)
{
    const Triangle neighbor = triangle(
        {0.0F, 0.0F, 0.0F},
        {2.0F, 0.0F, 0.0F},
        {1.0F, -1.0F, 0.0F});

    expect_collisions({base_triangle, neighbor}, {true, true});
}

TEST(CpuIntersections, CountsSharedVertexAsCollision)
{
    const Triangle neighbor = triangle(
        {2.0F, 0.0F, 0.0F},
        {3.0F, 0.0F, 0.0F},
        {2.0F, 1.0F, 0.0F});

    expect_collisions({base_triangle, neighbor}, {true, true});
}

TEST(CpuIntersections, DetectsNonCoplanarIntersection)
{
    const Triangle vertical = triangle(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 1.5F, 0.0F});

    expect_collisions({base_triangle, vertical}, {true, true});
}

TEST(CpuIntersections, RejectsTrianglesSeparatedByParallelPlanes)
{
    const Triangle above = triangle(
        {0.0F, 0.0F, 1.0F},
        {2.0F, 0.0F, 1.0F},
        {0.0F, 2.0F, 1.0F});

    expect_collisions({base_triangle, above}, {false, false});
}

TEST(CpuIntersections, RejectsDisjointAabbs)
{
    const Triangle far_away = triangle(
        {100.0F, 100.0F, 100.0F},
        {101.0F, 100.0F, 100.0F},
        {100.0F, 101.0F, 100.0F});

    expect_collisions({base_triangle, far_away}, {false, false});
}

TEST(CpuIntersections, MarksOnlyMembersOfCollidingPairs)
{
    const Triangle crossing = triangle(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 1.5F, 0.0F});
    const Triangle isolated = triangle(
        {10.0F, 10.0F, 0.0F},
        {11.0F, 10.0F, 0.0F},
        {10.0F, 11.0F, 0.0F});

    expect_collisions(
        {base_triangle, crossing, isolated},
        {true, true, false});
}

TEST(CpuIntersections, ResultDoesNotDependOnPairOrder)
{
    const Triangle vertical = triangle(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 1.5F, 0.0F});

    expect_collisions({vertical, base_triangle}, {true, true});
}

TEST(CpuIntersections, DetectsEqualDegeneratePoints)
{
    const Triangle first = point({1.0F, 2.0F, 3.0F});
    const Triangle second = point({1.0F, 2.0F, 3.0F});

    expect_collisions({first, second}, {true, true});
}

TEST(CpuIntersections, RejectsDifferentDegeneratePoints)
{
    const Triangle first = point({1.0F, 2.0F, 3.0F});
    const Triangle second = point({1.01F, 2.0F, 3.0F});

    expect_collisions({first, second}, {false, false});
}

TEST(CpuIntersections, DetectsPointInsideTriangle)
{
    expect_collisions(
        {base_triangle, point({0.5F, 0.5F, 0.0F})},
        {true, true});
}

TEST(CpuIntersections, RejectsPointOutsideTriangle)
{
    expect_collisions(
        {base_triangle, point({1.5F, 1.5F, 0.0F})},
        {false, false});
}

TEST(CpuIntersections, DetectsSegmentCrossingTrianglePlane)
{
    const Triangle crossing = segment(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F});

    expect_collisions({base_triangle, crossing}, {true, true});
}

TEST(CpuIntersections, DetectsCoplanarSegmentCrossingTriangle)
{
    const Triangle crossing = segment(
        {-1.0F, 0.5F, 0.0F},
        {1.0F, 0.5F, 0.0F});

    expect_collisions({base_triangle, crossing}, {true, true});
}

TEST(CpuIntersections, RejectsSegmentMissingTriangle)
{
    const Triangle missing = segment(
        {1.5F, 1.5F, -1.0F},
        {1.5F, 1.5F, 1.0F});

    expect_collisions({base_triangle, missing}, {false, false});
}

TEST(CpuIntersections, DetectsCrossingDegenerateSegments)
{
    const Triangle horizontal = segment(
        {-1.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F});
    const Triangle vertical = segment(
        {0.0F, -1.0F, 0.0F},
        {0.0F, 1.0F, 0.0F});

    expect_collisions({horizontal, vertical}, {true, true});
}

TEST(CpuIntersections, RejectsSkewDegenerateSegments)
{
    const Triangle first = segment(
        {-1.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F});
    const Triangle second = segment(
        {0.0F, -1.0F, 1.0F},
        {0.0F, 1.0F, 1.0F});

    expect_collisions({first, second}, {false, false});
}

TEST(CpuIntersections, HandlesSeveralTrianglesInSparseVoxels)
{
    const Triangle local_overlap = triangle(
        {0.25F, 0.25F, 0.0F},
        {0.75F, 0.25F, 0.0F},
        {0.25F, 0.75F, 0.0F});
    const Triangle remote = triangle(
        {1000.0F, 0.0F, 0.0F},
        {1002.0F, 0.0F, 0.0F},
        {1000.0F, 2.0F, 0.0F});
    const Triangle remote_overlap = triangle(
        {1000.25F, 0.25F, 0.0F},
        {1000.75F, 0.25F, 0.0F},
        {1000.25F, 0.75F, 0.0F});
    const Triangle isolated = triangle(
        {-1000.0F, 0.0F, 0.0F},
        {-999.0F, 0.0F, 0.0F},
        {-1000.0F, 1.0F, 0.0F});

    expect_collisions(
        {
            base_triangle,
            local_overlap,
            remote,
            remote_overlap,
            isolated,
        },
        {true, true, true, true, false});
}

TEST(CpuIntersections, DoesNotMergeSmallPositiveGap)
{
    const Triangle nearby = triangle(
        {2.001F, 0.0F, 0.0F},
        {3.0F, 0.0F, 0.0F},
        {2.001F, 1.0F, 0.0F});

    expect_collisions({base_triangle, nearby}, {false, false});
}

TEST(CpuIntersections, RejectsNonCoplanarTrianglesWithOverlappingAabbs)
{
    const Triangle outside = triangle(
        {1.5F, 1.5F, -1.0F},
        {1.5F, 1.5F, 1.0F},
        {2.0F, 0.5F, 1.0F});

    expect_collisions({base_triangle, outside}, {false, false});
}

TEST(CpuIntersections, RejectsNearlyParallelTriangleOutsideSharedPlane)
{
    const Triangle nearly_parallel = triangle(
        {2.1F, 0.0F, 0.0F},
        {0.0F, 2.1F, 0.0001F},
        {2.1F, 2.1F, 0.0001F});

    expect_collisions({base_triangle, nearly_parallel}, {false, false});
}

TEST(CpuIntersections, SatResultDoesNotDependOnVertexWinding)
{
    const Triangle reversed = triangle(
        {0.5F, 1.5F, 0.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 0.5F, -1.0F});

    expect_collisions({base_triangle, reversed}, {true, true});
}

TEST(CpuIntersections, DetectsDegeneratePointOnTriangleEdge)
{
    expect_collisions(
        {base_triangle, point({1.0F, 1.0F, 0.0F})},
        {true, true});
}

TEST(CpuIntersections, DetectsDegenerateSegmentInsideTriangle)
{
    const Triangle inside = segment(
        {0.25F, 0.25F, 0.0F},
        {0.75F, 0.25F, 0.0F});

    expect_collisions({base_triangle, inside}, {true, true});
}

TEST(CpuIntersections, DetectsCoplanarSegmentTouchingTriangleEdge)
{
    const Triangle touching = segment(
        {-1.0F, 1.0F, 0.0F},
        {0.0F, 1.0F, 0.0F});

    expect_collisions({base_triangle, touching}, {true, true});
}

TEST(CpuIntersections, DistinguishesOverlappingAndDisjointCollinearSegments)
{
    const Triangle first = segment(
        {0.0F, 0.0F, 0.0F},
        {2.0F, 0.0F, 0.0F});
    const Triangle overlapping = segment(
        {1.0F, 0.0F, 0.0F},
        {3.0F, 0.0F, 0.0F});
    const Triangle disjoint = segment(
        {4.0F, 0.0F, 0.0F},
        {5.0F, 0.0F, 0.0F});

    expect_collisions(
        {first, overlapping, disjoint},
        {true, true, false});
}

TEST(CpuIntersections, PreservesSeparationAfterLargeTranslation)
{
    const Triangle outside = triangle(
        {1.25F, 1.25F, 0.0F},
        {3.0F, 1.25F, 0.0F},
        {1.25F, 3.0F, 0.0F});
    constexpr Vec3 offset{1'000'000.0F, -1'000'000.0F, 500'000.0F};

    expect_collisions(
        {translated(base_triangle, offset), translated(outside, offset)},
        {false, false});
}

TEST(CpuIntersections, PreservesIntersectionAfterLargeTranslation)
{
    const Triangle vertical = triangle(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 1.5F, 0.0F});
    constexpr Vec3 offset{1'000'000.0F, -1'000'000.0F, 500'000.0F};

    expect_collisions(
        {translated(base_triangle, offset), translated(vertical, offset)},
        {true, true});
}

TEST(CpuIntersections, DoesNotCollapseSmallSeparatedTrianglesToPoints)
{
    const Triangle outside = triangle(
        {1.25F, 1.25F, 0.0F},
        {3.0F, 1.25F, 0.0F},
        {1.25F, 3.0F, 0.0F});

    expect_collisions(
        {scaled(base_triangle, 1.0e-9F), scaled(outside, 1.0e-9F)},
        {false, false});
}

TEST(CpuIntersections, DetectsSmallNonDegenerateIntersection)
{
    const Triangle vertical = triangle(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 1.5F, 0.0F});

    expect_collisions(
        {scaled(base_triangle, 1.0e-9F), scaled(vertical, 1.0e-9F)},
        {true, true});
}

TEST(CpuIntersections, ClassificationDoesNotDependOnOtherTriangleScale)
{
    const Triangle large_outside = triangle(
        {1.25F, 1.25F, 0.0F},
        {1'000'000.0F, 1.25F, 0.0F},
        {1.25F, 1'000'000.0F, 0.0F});

    expect_collisions({base_triangle, large_outside}, {false, false});
}

TEST(CpuIntersections, DegenerateSegmentsAreTranslationAndScaleInvariant)
{
    const Triangle horizontal = segment(
        {-1.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F});
    const Triangle vertical = segment(
        {0.0F, -1.0F, 0.0F},
        {0.0F, 1.0F, 0.0F});
    constexpr Vec3 offset{1'000'000.0F, 500'000.0F, -250'000.0F};

    expect_collisions(
        {
            translated(scaled(horizontal, 0.25F), offset),
            translated(scaled(vertical, 0.25F), offset),
        },
        {true, true});
}

TEST(CpuIntersections, RejectsNanAndInfinityBeforeBroadPhase)
{
    Triangle nan_triangle = base_triangle;
    nan_triangle.a.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        [[maybe_unused]] const auto result =
            hw3d::cpu_collisions({nan_triangle}),
        std::invalid_argument);

    Triangle infinite_triangle = base_triangle;
    infinite_triangle.c.z = std::numeric_limits<float>::infinity();
    EXPECT_THROW(
        [[maybe_unused]] const auto result =
            hw3d::cpu_collisions({base_triangle, infinite_triangle}),
        std::invalid_argument);
}

TEST(CpuIntersections, FixedRandomizedSetIsMetamorphic)
{
    std::uint32_t state = 0xc001d00dU;
    const auto next_coordinate = [&]() {
        state = state * 1'664'525U + 1'013'904'223U;
        return static_cast<float>(static_cast<int>(state % 33U) - 16);
    };

    std::vector<Triangle> original;
    original.reserve(40);
    for (int index = 0; index < 40; ++index) {
        original.push_back(triangle(
            {next_coordinate(), next_coordinate(), next_coordinate()},
            {next_coordinate(), next_coordinate(), next_coordinate()},
            {next_coordinate(), next_coordinate(), next_coordinate()}));
    }
    const std::vector<bool> expected = hw3d::cpu_collisions(original);

    std::vector<Triangle> transformed;
    transformed.reserve(original.size());
    for (const Triangle& value : original) {
        const Triangle permuted{value.b, value.c, value.a};
        transformed.push_back(scaled(permuted, 0x1p-10F));
    }
    EXPECT_EQ(hw3d::cpu_collisions(transformed), expected);
}

}
