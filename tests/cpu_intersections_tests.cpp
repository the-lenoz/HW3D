#include <gtest/gtest.h>

#include <algorithm>
#include <initializer_list>
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

}
