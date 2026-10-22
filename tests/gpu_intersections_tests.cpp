#include <gtest/gtest.h>

#include <glad/gl.h>

#include <cstddef>
#include <chrono>
#include <initializer_list>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "embedded_shaders.hpp"

import hw3d.configuration;
import hw3d.geometry;
import hw3d.gpu_intersections;
import hw3d.shader_program;
import hw3d.window_context;

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

Triangle translated(const Triangle& value, const Vec3 offset) noexcept
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

void clear_gl_errors() noexcept
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

class GpuIntersectionsTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        window_ = std::make_unique<hw3d::WindowContext>(
            64,
            64,
            "GPU intersections tests");
    }

    static void TearDownTestSuite()
    {
        window_.reset();
    }

    void SetUp() override
    {
        clear_gl_errors();
    }

    void expect_collisions(
        std::vector<Triangle> triangles,
        const std::initializer_list<bool> expected)
    {
        const auto bounds = static_motion_bounds(triangles);
        hw3d::GpuIntersectionDetector detector{triangles, bounds};
        detector.update();
        ASSERT_EQ(triangles.size(), expected.size());
        std::size_t index = 0;
        for (const bool value : expected) {
            EXPECT_EQ(triangles[index].highlighted, value)
                << "triangle index: " << index;
            ++index;
        }
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
    }

    static inline std::unique_ptr<hw3d::WindowContext> window_;
};

const Triangle base_triangle = triangle(
    {0.0F, 0.0F, 0.0F},
    {2.0F, 0.0F, 0.0F},
    {0.0F, 2.0F, 0.0F});

TEST_F(GpuIntersectionsTest, EmbeddedComputeShaderLinksThroughSharedProgram)
{
    const auto program = hw3d::ShaderProgram::compute(
        hw3d::shaders::intersections_comp_glsl);

    program.use();
    GLint bound_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &bound_program);
    EXPECT_GT(bound_program, 0);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
    glUseProgram(0);
}

TEST_F(GpuIntersectionsTest, ReturnsEmptyResultForEmptyInput)
{
    expect_collisions({}, {});
}

TEST_F(GpuIntersectionsTest, SingleTriangleDoesNotCollide)
{
    expect_collisions({base_triangle}, {false});
}

TEST_F(GpuIntersectionsTest, ClearsPreviousHighlights)
{
    const Triangle far = triangle(
        {10.0F, 10.0F, 10.0F},
        {11.0F, 10.0F, 10.0F},
        {10.0F, 11.0F, 10.0F});
    std::vector<Triangle> triangles{base_triangle, far};
    triangles[0].highlighted = true;
    triangles[1].highlighted = true;

    const auto bounds = static_motion_bounds(triangles);
    hw3d::GpuIntersectionDetector detector{triangles, bounds};
    detector.update();

    EXPECT_FALSE(triangles[0].highlighted);
    EXPECT_FALSE(triangles[1].highlighted);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, ReusesBuffersAndStaticMotionPairsAcrossUpdates)
{
    const Triangle far = triangle(
        {50.0F, 0.0F, 0.0F},
        {52.0F, 0.0F, 0.0F},
        {50.0F, 2.0F, 0.0F});
    std::vector<Triangle> triangles{base_triangle, far};
    const std::vector<hw3d::Aabb> motion_bounds{
        {{-1.0F, -1.0F, -1.0F}, {3.0F, 3.0F, 1.0F}},
        {{-1.0F, -1.0F, -1.0F}, {53.0F, 3.0F, 1.0F}},
    };
    hw3d::GpuIntersectionDetector detector{triangles, motion_bounds};

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
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, PairBuffersSurviveRuntimeVectorReallocation)
{
    std::vector<Triangle> triangles{base_triangle, base_triangle};
    const auto bounds = static_motion_bounds(triangles);
    hw3d::GpuIntersectionDetector detector{triangles, bounds};

    const Triangle* const old_data = triangles.data();
    triangles.reserve(triangles.capacity() + 100);
    ASSERT_NE(triangles.data(), old_data);

    detector.update();
    EXPECT_TRUE(triangles[0].highlighted);
    EXPECT_TRUE(triangles[1].highlighted);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, ReusesDensePairGraphWithoutStaleHighlights)
{
    constexpr std::size_t triangle_count = 5;
    std::vector<Triangle> triangles(triangle_count, base_triangle);
    const std::vector<hw3d::Aabb> motion_bounds(
        triangle_count,
        {{-100.0F, -100.0F, -1.0F}, {100.0F, 100.0F, 1.0F}});
    hw3d::GpuIntersectionDetector detector{triangles, motion_bounds};

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
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, VoxelFallbackUpdatesFlagsAndPreservesInputOrder)
{
    const Triangle far = translated(base_triangle, {50.0F, 0.0F, 0.0F});
    const std::vector<hw3d::Aabb> motion_bounds{
        {{-2.0F, -2.0F, -1.0F}, {55.0F, 5.0F, 1.0F}},
        {{-2.0F, -2.0F, -1.0F}, {55.0F, 5.0F, 1.0F}},
        {{48.0F, -2.0F, -1.0F}, {55.0F, 5.0F, 1.0F}},
    };
    for (const std::size_t budget : {std::size_t{0}, std::size_t{1}}) {
        std::vector<Triangle> triangles{base_triangle, base_triangle, far};
        hw3d::GpuIntersectionDetector detector{
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
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, EmptyPairCacheStillValidatesCoordinatesAndClearsFlags)
{
    std::vector<Triangle> triangles{base_triangle};
    triangles[0].highlighted = true;
    const auto bounds = static_motion_bounds(triangles);
    hw3d::GpuIntersectionDetector detector{triangles, bounds};

    detector.update();
    EXPECT_FALSE(triangles[0].highlighted);

    triangles[0].a.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(detector.update(), std::invalid_argument);
}

TEST_F(GpuIntersectionsTest, RejectsPairBudgetAboveSupportedMaximum)
{
    std::vector<Triangle> triangles{base_triangle};
    const auto bounds = static_motion_bounds(triangles);

    EXPECT_THROW(
        hw3d::GpuIntersectionDetector(
            triangles,
            bounds,
            hw3d::maximum_cached_triangle_pair_count + 1),
        std::invalid_argument);
}

TEST_F(GpuIntersectionsTest, TracksRotationFromInitialGeometryWithScaledAxis)
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
    hw3d::GpuIntersectionDetector detector{triangles, motion_bounds};

    detector.update();
    EXPECT_FALSE(triangles[0].highlighted);
    EXPECT_FALSE(triangles[1].highlighted);

    hw3d::update_vertices(initial, triangles, std::chrono::duration<double>{1.0});
    detector.update();
    EXPECT_TRUE(triangles[0].highlighted);
    EXPECT_TRUE(triangles[1].highlighted);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, ValidatesPersistentSceneShape)
{
    std::vector<Triangle> triangles{base_triangle};
    const auto bounds = static_motion_bounds(triangles);
    hw3d::GpuIntersectionDetector detector{triangles, bounds};

    triangles.push_back(base_triangle);
    EXPECT_THROW(detector.update(), std::logic_error);

    std::vector<Triangle> separate{base_triangle};
    const std::vector<hw3d::Aabb> no_bounds;
    EXPECT_THROW(
        hw3d::GpuIntersectionDetector(separate, no_bounds),
        std::invalid_argument);
}

TEST_F(GpuIntersectionsTest, DetectsIdenticalTriangles)
{
    expect_collisions({base_triangle, base_triangle}, {true, true});
}

TEST_F(GpuIntersectionsTest, DetectsIdenticalSmallTriangles)
{
    const auto scale = [](const Triangle& value, const float factor) {
        return triangle(
            {value.a.x * factor, value.a.y * factor, value.a.z * factor},
            {value.b.x * factor, value.b.y * factor, value.b.z * factor},
            {value.c.x * factor, value.c.y * factor, value.c.z * factor});
    };

    const Triangle small = scale(base_triangle, 0.01F);
    expect_collisions({small, small}, {true, true});
}

TEST_F(GpuIntersectionsTest, DetectsCoplanarContainment)
{
    const Triangle inner = triangle(
        {0.25F, 0.25F, 0.0F},
        {0.75F, 0.25F, 0.0F},
        {0.25F, 0.75F, 0.0F});

    expect_collisions({base_triangle, inner}, {true, true});
}

TEST_F(GpuIntersectionsTest, RejectsCoplanarTrianglesWithOverlappingAabbs)
{
    const Triangle outside = triangle(
        {1.25F, 1.25F, 0.0F},
        {3.0F, 1.25F, 0.0F},
        {1.25F, 3.0F, 0.0F});

    expect_collisions({base_triangle, outside}, {false, false});
}

TEST_F(GpuIntersectionsTest, CountsSharedEdgeAsCollision)
{
    const Triangle neighbor = triangle(
        {0.0F, 0.0F, 0.0F},
        {2.0F, 0.0F, 0.0F},
        {1.0F, -1.0F, 0.0F});

    expect_collisions({base_triangle, neighbor}, {true, true});
}

TEST_F(GpuIntersectionsTest, CountsSharedVertexAsCollision)
{
    const Triangle neighbor = triangle(
        {2.0F, 0.0F, 0.0F},
        {3.0F, 0.0F, 0.0F},
        {2.0F, 1.0F, 0.0F});

    expect_collisions({base_triangle, neighbor}, {true, true});
}

TEST_F(GpuIntersectionsTest, DetectsNonCoplanarIntersection)
{
    const Triangle vertical = triangle(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 1.5F, 0.0F});

    expect_collisions({base_triangle, vertical}, {true, true});
}

TEST_F(GpuIntersectionsTest, RejectsTrianglesSeparatedByParallelPlanes)
{
    const Triangle above = triangle(
        {0.0F, 0.0F, 1.0F},
        {2.0F, 0.0F, 1.0F},
        {0.0F, 2.0F, 1.0F});

    expect_collisions({base_triangle, above}, {false, false});
}

TEST_F(GpuIntersectionsTest, RejectsDisjointAabbs)
{
    const Triangle far_away = triangle(
        {100.0F, 100.0F, 100.0F},
        {101.0F, 100.0F, 100.0F},
        {100.0F, 101.0F, 100.0F});

    expect_collisions({base_triangle, far_away}, {false, false});
}

TEST_F(GpuIntersectionsTest, MarksOnlyMembersOfCollidingPairs)
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

TEST_F(GpuIntersectionsTest, ResultDoesNotDependOnPairOrder)
{
    const Triangle vertical = triangle(
        {0.5F, 0.5F, -1.0F},
        {0.5F, 0.5F, 1.0F},
        {0.5F, 1.5F, 0.0F});

    expect_collisions({vertical, base_triangle}, {true, true});
}

TEST_F(GpuIntersectionsTest, HandlesSparseVoxelsAndRestoresInputOrder)
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
            remote_overlap,
            isolated,
            base_triangle,
            remote,
            local_overlap,
        },
        {true, false, true, true, true});
}

TEST_F(GpuIntersectionsTest, DoesNotMergeSmallPositiveGap)
{
    const Triangle nearby = triangle(
        {2.001F, 0.0F, 0.0F},
        {3.0F, 0.0F, 0.0F},
        {2.001F, 1.0F, 0.0F});

    expect_collisions({base_triangle, nearby}, {false, false});
}

TEST_F(GpuIntersectionsTest, PreservesResultAfterTriangleWindingChanges)
{
    const Triangle reversed = triangle(
        base_triangle.a,
        base_triangle.c,
        base_triangle.b);

    expect_collisions({base_triangle, reversed}, {true, true});
}

TEST_F(GpuIntersectionsTest, DispatchesMotionPairsAcrossMultipleWorkGroups)
{
    constexpr std::size_t triangle_count = 17;
    std::vector<Triangle> triangles(triangle_count, base_triangle);

    const auto bounds = static_motion_bounds(triangles);
    hw3d::GpuIntersectionDetector detector{triangles, bounds};
    detector.update();

    for (std::size_t index = 0; index < triangles.size(); ++index) {
        EXPECT_TRUE(triangles[index].highlighted) << "triangle index: " << index;
    }
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, DispatchesAtAndAcrossPairWorkGroupBoundary)
{
    const auto run_case = [&](const std::size_t leaf_count) {
        std::vector<Triangle> triangles;
        triangles.reserve(leaf_count + 1);
        for (std::size_t index = 0; index < leaf_count; ++index) {
            const float x = 2.0F + static_cast<float>(index) * 10.0F;
            triangles.push_back(triangle(
                {x, 1.0F, 0.0F},
                {x + 1.0F, 1.0F, 0.0F},
                {x, 2.0F, 0.0F}));
        }
        triangles.push_back(triangle(
            {0.0F, 0.0F, 0.0F},
            {1000.0F, 0.0F, 0.0F},
            {0.0F, 1000.0F, 0.0F}));

        const auto bounds = static_motion_bounds(triangles);
        const auto candidates =
            hw3d::make_motion_pair_candidates(bounds, leaf_count);
        ASSERT_TRUE(candidates.pairs.has_value());
        ASSERT_EQ(candidates.pairs->size(), leaf_count);

        hw3d::GpuIntersectionDetector detector{triangles, bounds};
        detector.update();
        for (std::size_t index = 0; index < triangles.size(); ++index) {
            EXPECT_TRUE(triangles[index].highlighted)
                << "leaf count: " << leaf_count
                << ", triangle index: " << index;
        }
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
    };

    run_case(64);
    run_case(65);
}

}
