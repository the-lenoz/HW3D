#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

import hw3d.configuration;
import hw3d.geometry;
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

std::vector<hw3d::Aabb> static_motion_bounds(
    const std::vector<Triangle>& triangles)
{
    std::vector<hw3d::Aabb> result;
    result.reserve(triangles.size());
    for (const Triangle& value : triangles) {
        result.push_back(hw3d::aabb(value));
    }
    return result;
}

void expect_collisions(
    std::vector<Triangle> triangles,
    const std::initializer_list<bool> expected)
{
    const auto bounds = static_motion_bounds(triangles);
    hw3d::CpuIntersectionDetector detector{triangles, bounds};
    detector.update();

    ASSERT_EQ(triangles.size(), expected.size());
    std::size_t index = 0;
    for (const bool value : expected) {
        EXPECT_EQ(triangles[index].highlighted, value)
            << "triangle index: " << index;
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

TEST(CpuIntersections, ClearsPreviousHighlights)
{
    const Triangle far = triangle(
        {10.0F, 10.0F, 10.0F},
        {11.0F, 10.0F, 10.0F},
        {10.0F, 11.0F, 10.0F});
    std::vector<Triangle> triangles{base_triangle, far};
    triangles[0].highlighted = true;
    triangles[1].highlighted = true;

    const auto bounds = static_motion_bounds(triangles);
    hw3d::CpuIntersectionDetector detector{triangles, bounds};
    detector.update();

    EXPECT_FALSE(triangles[0].highlighted);
    EXPECT_FALSE(triangles[1].highlighted);
}

TEST(CpuIntersections, ReusesStaticMotionPairsAcrossVertexUpdates)
{
    const Triangle far = translated(base_triangle, {50.0F, 0.0F, 0.0F});
    std::vector<Triangle> triangles{base_triangle, far};
    const std::vector<hw3d::Aabb> motion_bounds{
        {{-1.0F, -1.0F, -1.0F}, {3.0F, 3.0F, 1.0F}},
        {{-1.0F, -1.0F, -1.0F}, {53.0F, 3.0F, 1.0F}},
    };
    hw3d::CpuIntersectionDetector detector{triangles, motion_bounds};

    detector.update();
    EXPECT_FALSE(triangles[0].highlighted);
    EXPECT_FALSE(triangles[1].highlighted);

    triangles[1] = base_triangle;
    detector.update();
    EXPECT_TRUE(triangles[0].highlighted);
    EXPECT_TRUE(triangles[1].highlighted);

    triangles[1] = far;
    detector.update();
    EXPECT_FALSE(triangles[0].highlighted);
    EXPECT_FALSE(triangles[1].highlighted);
}

TEST(CpuIntersections, PairCacheSurvivesRuntimeVectorReallocation)
{
    std::vector<Triangle> triangles{base_triangle, base_triangle};
    const auto bounds = static_motion_bounds(triangles);
    hw3d::CpuIntersectionDetector detector{triangles, bounds};

    const Triangle* const old_data = triangles.data();
    triangles.reserve(triangles.capacity() + 100);
    ASSERT_NE(triangles.data(), old_data);

    detector.update();
    EXPECT_TRUE(triangles[0].highlighted);
    EXPECT_TRUE(triangles[1].highlighted);
}

TEST(CpuIntersections, ReusesDensePairGraphWithoutStaleHighlights)
{
    constexpr std::size_t triangle_count = 5;
    std::vector<Triangle> triangles(triangle_count, base_triangle);
    const std::vector<hw3d::Aabb> motion_bounds(
        triangle_count,
        {{-100.0F, -100.0F, -1.0F}, {100.0F, 100.0F, 1.0F}});
    hw3d::CpuIntersectionDetector detector{triangles, motion_bounds};

    detector.update();
    for (std::size_t index = 0; index < triangle_count; ++index) {
        EXPECT_TRUE(triangles[index].highlighted)
            << "triangle index: " << index;
        triangles[index] = translated(
            base_triangle,
            {static_cast<float>(index) * 10.0F, 0.0F, 0.0F});
    }

    detector.update();
    for (std::size_t index = 0; index < triangle_count; ++index) {
        EXPECT_FALSE(triangles[index].highlighted)
            << "triangle index: " << index;
    }
}

TEST(CpuIntersections, VoxelFallbackUpdatesFlagsAndPreservesInputOrder)
{
    const Triangle far = translated(base_triangle, {50.0F, 0.0F, 0.0F});
    const std::vector<hw3d::Aabb> motion_bounds{
        {{-2.0F, -2.0F, -1.0F}, {55.0F, 5.0F, 1.0F}},
        {{-2.0F, -2.0F, -1.0F}, {55.0F, 5.0F, 1.0F}},
        {{48.0F, -2.0F, -1.0F}, {55.0F, 5.0F, 1.0F}},
    };
    for (const std::size_t budget : {std::size_t{0}, std::size_t{1}}) {
        std::vector<Triangle> triangles{base_triangle, base_triangle, far};
        hw3d::CpuIntersectionDetector detector{
            triangles, motion_bounds, budget};

        detector.update();
        EXPECT_TRUE(triangles[0].highlighted) << "budget: " << budget;
        EXPECT_TRUE(triangles[1].highlighted) << "budget: " << budget;
        EXPECT_FALSE(triangles[2].highlighted) << "budget: " << budget;

        triangles[1] = far;
        detector.update();
        EXPECT_FALSE(triangles[0].highlighted) << "budget: " << budget;
        EXPECT_TRUE(triangles[1].highlighted) << "budget: " << budget;
        EXPECT_TRUE(triangles[2].highlighted) << "budget: " << budget;
    }
}

TEST(CpuIntersections, EmptyPairCacheStillValidatesCoordinatesAndClearsFlags)
{
    std::vector<Triangle> triangles{base_triangle};
    triangles[0].highlighted = true;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::CpuIntersectionDetector detector{triangles, bounds};

    detector.update();
    EXPECT_FALSE(triangles[0].highlighted);

    triangles[0].a.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(detector.update(), std::invalid_argument);
}

TEST(CpuIntersections, RejectsPairBudgetAboveSupportedMaximum)
{
    std::vector<Triangle> triangles{base_triangle};
    const auto bounds = static_motion_bounds(triangles);

    EXPECT_THROW(
        hw3d::CpuIntersectionDetector(
            triangles,
            bounds,
            hw3d::maximum_cached_triangle_pair_count + 1),
        std::invalid_argument);
}

TEST(CpuIntersections, TracksRotationFromInitialGeometryWithScaledAxis)
{
    const hw3d::InitialTriangle stationary{
        .a = base_triangle.a,
        .b = base_triangle.b,
        .c = base_triangle.c,
    };
    const hw3d::InitialTriangle moving{
        .a = {10.0F, 0.0F, 0.0F},
        .b = {8.0F, 0.0F, 0.0F},
        .c = {10.0F, -2.0F, 0.0F},
        .rotation_axis_origin = {5.0F, 0.0F, 0.0F},
        .rotation_axis_direction = {0.0F, 0.0F, 5.0F},
        .angular_velocity_radians_per_second = std::numbers::pi_v<float>,
    };
    const std::vector initial{stationary, moving};
    std::vector<Triangle> triangles(initial.size());
    std::vector<hw3d::Aabb> motion_bounds(initial.size());
    hw3d::calculate_motion_bounds(initial, motion_bounds);
    hw3d::update_vertices(initial, triangles, std::chrono::duration<double>{0.0});
    hw3d::CpuIntersectionDetector detector{triangles, motion_bounds};

    detector.update();
    EXPECT_FALSE(triangles[0].highlighted);
    EXPECT_FALSE(triangles[1].highlighted);

    hw3d::update_vertices(initial, triangles, std::chrono::duration<double>{1.0});
    detector.update();
    EXPECT_TRUE(triangles[0].highlighted);
    EXPECT_TRUE(triangles[1].highlighted);
}

TEST(CpuIntersections, ValidatesPersistentSceneShape)
{
    std::vector<Triangle> triangles{base_triangle};
    const auto bounds = static_motion_bounds(triangles);
    hw3d::CpuIntersectionDetector detector{triangles, bounds};

    triangles.push_back(base_triangle);
    EXPECT_THROW(detector.update(), std::logic_error);

    std::vector<Triangle> separate{base_triangle};
    const std::vector<hw3d::Aabb> no_bounds;
    EXPECT_THROW(
        hw3d::CpuIntersectionDetector(separate, no_bounds),
        std::invalid_argument);
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
    std::vector<Triangle> nan_triangles{nan_triangle};
    const std::vector nan_bounds{hw3d::Aabb{}};
    hw3d::CpuIntersectionDetector nan_detector{nan_triangles, nan_bounds};
    EXPECT_THROW(
        nan_detector.update(),
        std::invalid_argument);

    Triangle infinite_triangle = base_triangle;
    infinite_triangle.c.z = std::numeric_limits<float>::infinity();
    std::vector<Triangle> infinite_triangles{
        base_triangle,
        infinite_triangle,
    };
    const std::vector infinite_bounds{
        hw3d::aabb(base_triangle),
        hw3d::aabb(base_triangle),
    };
    hw3d::CpuIntersectionDetector infinite_detector{
        infinite_triangles, infinite_bounds};
    EXPECT_THROW(
        infinite_detector.update(),
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
    const auto original_bounds = static_motion_bounds(original);
    hw3d::CpuIntersectionDetector original_detector{
        original, original_bounds};
    original_detector.update();

    std::vector<Triangle> transformed;
    transformed.reserve(original.size());
    for (const Triangle& value : original) {
        const Triangle permuted{value.b, value.c, value.a};
        transformed.push_back(scaled(permuted, 0x1p-10F));
    }
    const auto transformed_bounds = static_motion_bounds(transformed);
    hw3d::CpuIntersectionDetector transformed_detector{
        transformed, transformed_bounds};
    transformed_detector.update();
    ASSERT_EQ(transformed.size(), original.size());
    for (std::size_t index = 0; index < transformed.size(); ++index) {
        EXPECT_EQ(transformed[index].highlighted, original[index].highlighted)
            << "triangle index: " << index;
    }
}

}
