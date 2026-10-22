#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

import hw3d.configuration;
import hw3d.geometry;

namespace {

hw3d::InitialTriangle rotating_triangle(
    const float angular_velocity = -std::numbers::pi_v<float> / 2.0F)
{
    return {
        .a = {1.0F, 0.0F, 0.0F},
        .b = {0.0F, 1.0F, 0.0F},
        .c = {0.0F, 0.0F, 1.0F},
        .rotation_axis_origin = {0.0F, 0.0F, 0.0F},
        .rotation_axis_direction = {0.0F, 0.0F, 1.0F},
        .angular_velocity_radians_per_second = angular_velocity,
    };
}

TEST(Geometry, UpdatesVerticesFromAbsoluteTime)
{
    const std::vector initial{rotating_triangle()};
    std::vector<hw3d::Triangle> current(initial.size());

    hw3d::update_vertices(initial, current, std::chrono::duration<double>{1.0});

    EXPECT_NEAR(current[0].a.x, 0.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].a.y, -1.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].b.x, 1.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].b.y, 0.0F, 1.0e-6F);
    EXPECT_FLOAT_EQ(current[0].c.z, 1.0F);
}

TEST(Geometry, PreservesExactVerticesAtZeroTimeAndForStationaryTriangles)
{
    const hw3d::InitialTriangle rotating = rotating_triangle();
    hw3d::InitialTriangle stationary = rotating;
    stationary.angular_velocity_radians_per_second = 0.0F;
    const std::vector initial{rotating, stationary};
    std::vector<hw3d::Triangle> current(initial.size());

    hw3d::update_vertices(initial, current, std::chrono::duration<double>{0.0});
    EXPECT_EQ(current[0].a.x, rotating.a.x);
    EXPECT_EQ(current[0].b.y, rotating.b.y);

    hw3d::update_vertices(initial, current, std::chrono::duration<double>{123.0});
    EXPECT_EQ(current[1].a.x, stationary.a.x);
    EXPECT_EQ(current[1].b.y, stationary.b.y);
    EXPECT_EQ(current[1].c.z, stationary.c.z);
}

TEST(Geometry, RotatesRadiusVectorsAroundAnOffsetAxis)
{
    hw3d::InitialTriangle source = rotating_triangle();
    source.a = {2.0F, 1.0F, 0.0F};
    source.b = {1.0F, 2.0F, 0.0F};
    source.c = {1.0F, 1.0F, 3.0F};
    source.rotation_axis_origin = {1.0F, 1.0F, 0.0F};
    const std::vector initial{source};
    std::vector<hw3d::Triangle> current(1);

    hw3d::update_vertices(initial, current, std::chrono::duration<double>{1.0});

    EXPECT_NEAR(current[0].a.x, 1.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].a.y, 0.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].b.x, 2.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].b.y, 1.0F, 1.0e-6F);
    EXPECT_FLOAT_EQ(current[0].c.z, 3.0F);
}

TEST(Geometry, RotatesAroundAnArbitraryNormalizedAxis)
{
    hw3d::InitialTriangle source = rotating_triangle(
        -2.0F * std::numbers::pi_v<float> / 3.0F);
    const float component = 1.0F / std::sqrt(3.0F);
    source.rotation_axis_direction = {component, component, component};
    const std::vector initial{source};
    std::vector<hw3d::Triangle> current(1);

    hw3d::update_vertices(initial, current, std::chrono::duration<double>{1.0});

    EXPECT_NEAR(current[0].a.x, 0.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].a.y, 0.0F, 1.0e-6F);
    EXPECT_NEAR(current[0].a.z, 1.0F, 1.0e-6F);
}

TEST(Geometry, ScaledAxisProducesTheSameRotationAsNormalizedAxis)
{
    hw3d::InitialTriangle normalized = rotating_triangle();
    normalized.rotation_axis_origin = {3.0F, -2.0F, 1.0F};
    normalized.a = {4.0F, -2.0F, 1.0F};
    normalized.b = {3.0F, -1.0F, 1.0F};
    normalized.c = {3.0F, -2.0F, 2.0F};
    hw3d::InitialTriangle scaled = normalized;
    scaled.rotation_axis_direction = {0.0F, 0.0F, 5.0F};
    const std::vector initial{normalized, scaled};
    std::vector<hw3d::Triangle> current(initial.size());

    hw3d::update_vertices(initial, current, std::chrono::duration<double>{1.0});

    EXPECT_NEAR(current[0].a.x, current[1].a.x, 1.0e-6F);
    EXPECT_NEAR(current[0].a.y, current[1].a.y, 1.0e-6F);
    EXPECT_NEAR(current[0].b.x, current[1].b.x, 1.0e-6F);
    EXPECT_NEAR(current[0].b.y, current[1].b.y, 1.0e-6F);
    EXPECT_NEAR(current[0].c.z, current[1].c.z, 1.0e-6F);
}

TEST(Geometry, UpdatedVerticesRemainInsideFullRotationBounds)
{
    hw3d::InitialTriangle source = rotating_triangle(1.25F);
    source.rotation_axis_origin = {10.0F, -7.0F, 3.0F};
    source.rotation_axis_direction = {2.0F, -3.0F, 4.0F};
    source.a = {14.0F, -2.0F, 8.0F};
    source.b = {8.0F, -10.0F, 6.0F};
    source.c = {11.0F, -4.0F, -2.0F};
    const std::vector initial{source};
    std::vector<hw3d::Triangle> current(1);
    const hw3d::Aabb bounds = hw3d::motion_aabb(source);

    const auto expect_inside = [&](const hw3d::Vec3& point) {
        EXPECT_GE(point.x, bounds.minimum.x);
        EXPECT_GE(point.y, bounds.minimum.y);
        EXPECT_GE(point.z, bounds.minimum.z);
        EXPECT_LE(point.x, bounds.maximum.x);
        EXPECT_LE(point.y, bounds.maximum.y);
        EXPECT_LE(point.z, bounds.maximum.z);
    };
    for (int sample = 0; sample <= 64; ++sample) {
        const double time = static_cast<double>(sample) * 0.1;
        hw3d::update_vertices(
            initial,
            current,
            std::chrono::duration<double>{time});
        expect_inside(current[0].a);
        expect_inside(current[0].b);
        expect_inside(current[0].c);
    }
}

TEST(Geometry, UsesSourceVerticesWithoutAccumulatingRotationError)
{
    const std::vector initial{rotating_triangle()};
    std::vector<hw3d::Triangle> first(1);
    std::vector<hw3d::Triangle> after_full_turn(1);
    first[0].highlighted = true;

    hw3d::update_vertices(initial, first, std::chrono::duration<double>{0.25});
    hw3d::update_vertices(
        initial,
        after_full_turn,
        std::chrono::duration<double>{4.25});

    EXPECT_TRUE(first[0].highlighted);
    EXPECT_NEAR(first[0].a.x, after_full_turn[0].a.x, 1.0e-6F);
    EXPECT_NEAR(first[0].a.y, after_full_turn[0].a.y, 1.0e-6F);
    EXPECT_NEAR(first[0].a.z, after_full_turn[0].a.z, 1.0e-6F);
}

TEST(Geometry, RejectsInvalidUpdateArguments)
{
    const std::vector initial{rotating_triangle()};
    std::vector<hw3d::Triangle> empty;
    std::vector<hw3d::Triangle> current(1);

    EXPECT_THROW(
        hw3d::update_vertices(initial, empty, std::chrono::duration<double>{0.0}),
        std::invalid_argument);
    EXPECT_THROW(
        hw3d::update_vertices(initial, current, std::chrono::duration<double>{-1.0}),
        std::invalid_argument);
    EXPECT_THROW(
        hw3d::update_vertices(
            initial,
            current,
            std::chrono::duration<double>{
                std::numeric_limits<double>::quiet_NaN()}),
        std::invalid_argument);

    auto invalid_axis = initial;
    invalid_axis[0].rotation_axis_direction = {};
    EXPECT_THROW(
        hw3d::update_vertices(
            invalid_axis,
            current,
            std::chrono::duration<double>{1.0}),
        std::invalid_argument);
    invalid_axis[0].rotation_axis_direction.x =
        std::numeric_limits<float>::infinity();
    EXPECT_THROW(
        hw3d::update_vertices(
            invalid_axis,
            current,
            std::chrono::duration<double>{1.0}),
        std::invalid_argument);

    invalid_axis[0] = initial[0];
    invalid_axis[0].rotation_axis_origin.y =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        hw3d::update_vertices(
            invalid_axis,
            current,
            std::chrono::duration<double>{1.0}),
        std::invalid_argument);

    invalid_axis[0].angular_velocity_radians_per_second = 0.0F;
    EXPECT_NO_THROW(hw3d::update_vertices(
        invalid_axis,
        current,
        std::chrono::duration<double>{1.0}));
}

TEST(Geometry, BuildsTriangleAabb)
{
    const hw3d::Triangle triangle{
        {-2.0F, 4.0F, 1.0F},
        {3.0F, -1.0F, 7.0F},
        {0.0F, 2.0F, -5.0F},
    };

    const hw3d::Aabb bounds = hw3d::aabb(triangle);

    EXPECT_FLOAT_EQ(bounds.minimum.x, -2.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.y, -1.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.z, -5.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.x, 3.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.y, 4.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.z, 7.0F);
}

TEST(Geometry, UsesStaticAabbForExactlyZeroAngularVelocity)
{
    hw3d::InitialTriangle triangle{
        {1.0F, 0.0F, 0.0F},
        {0.0F, 2.0F, 0.0F},
        {0.0F, 0.0F, 3.0F},
    };
    triangle.rotation_axis_origin = {0.0F, 0.0F, 0.0F};
    triangle.rotation_axis_direction = {0.0F, 0.0F, 1.0F};
    triangle.angular_velocity_radians_per_second = 0.0F;

    const hw3d::Aabb bounds = hw3d::motion_aabb(triangle);

    EXPECT_FLOAT_EQ(bounds.minimum.x, 0.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.y, 0.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.z, 0.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.x, 1.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.y, 2.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.z, 3.0F);
}

TEST(Geometry, BoundsFullRotationOfTriangleVertices)
{
    hw3d::InitialTriangle triangle{
        {1.0F, 0.0F, 0.0F},
        {0.0F, 2.0F, 0.0F},
        {0.0F, 0.0F, 3.0F},
    };
    triangle.rotation_axis_origin = {0.0F, 0.0F, 0.0F};
    triangle.rotation_axis_direction = {0.0F, 0.0F, 1.0F};
    triangle.angular_velocity_radians_per_second = -1.0F;

    const hw3d::Aabb bounds = hw3d::motion_aabb(triangle);

    EXPECT_FLOAT_EQ(bounds.minimum.x, -2.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.y, -2.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.z, 0.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.x, 2.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.y, 2.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.z, 3.0F);

    const std::vector grid_bounds{bounds};
    const hw3d::VoxelGrid grid = hw3d::make_voxel_grid(grid_bounds);
    EXPECT_FLOAT_EQ(grid.voxel_size.x, 4.0F);
    EXPECT_FLOAT_EQ(grid.voxel_size.y, 4.0F);
    EXPECT_FLOAT_EQ(grid.voxel_size.z, 3.0F);
}

TEST(Geometry, AccountsForAxisOriginAndNormalizesDirection)
{
    hw3d::InitialTriangle triangle{
        {11.0F, 20.0F, 30.0F},
        {10.0F, 22.0F, 30.0F},
        {10.0F, 20.0F, 33.0F},
    };
    triangle.rotation_axis_origin = {10.0F, 20.0F, 30.0F};
    triangle.rotation_axis_direction = {0.0F, 0.0F, 5.0F};
    triangle.angular_velocity_radians_per_second = -1.0F;

    const hw3d::Aabb bounds = hw3d::motion_aabb(triangle);

    EXPECT_FLOAT_EQ(bounds.minimum.x, 8.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.y, 18.0F);
    EXPECT_FLOAT_EQ(bounds.minimum.z, 30.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.x, 12.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.y, 22.0F);
    EXPECT_FLOAT_EQ(bounds.maximum.z, 33.0F);
}

TEST(Geometry, CalculatesPreallocatedMotionBounds)
{
    hw3d::InitialTriangle stationary = rotating_triangle(0.0F);
    stationary.a = {-2.0F, 1.0F, 3.0F};
    const std::vector initial{rotating_triangle(), stationary};
    std::vector<hw3d::Aabb> bounds(initial.size());

    hw3d::calculate_motion_bounds(initial, bounds);

    EXPECT_FLOAT_EQ(bounds[0].minimum.x, -1.0F);
    EXPECT_FLOAT_EQ(bounds[0].maximum.y, 1.0F);
    EXPECT_FLOAT_EQ(bounds[1].minimum.x, -2.0F);
    EXPECT_FLOAT_EQ(bounds[1].maximum.z, 3.0F);
}

TEST(Geometry, RejectsInvalidMotionBoundInputs)
{
    std::vector initial{rotating_triangle()};
    std::vector<hw3d::Aabb> empty;
    std::vector<hw3d::Aabb> output(1);

    EXPECT_THROW(
        hw3d::calculate_motion_bounds(initial, empty),
        std::invalid_argument);

    initial[0].a.x = std::numeric_limits<float>::infinity();
    EXPECT_THROW(
        hw3d::calculate_motion_bounds(initial, output),
        std::invalid_argument);

    initial[0] = rotating_triangle();
    initial[0].rotation_axis_origin.y =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        hw3d::calculate_motion_bounds(initial, output),
        std::invalid_argument);

    initial[0] = rotating_triangle();
    initial[0].rotation_axis_direction = {};
    EXPECT_THROW(
        hw3d::calculate_motion_bounds(initial, output),
        std::invalid_argument);

    initial[0] = rotating_triangle();
    initial[0].angular_velocity_radians_per_second =
        std::numeric_limits<float>::infinity();
    EXPECT_THROW(
        hw3d::calculate_motion_bounds(initial, output),
        std::invalid_argument);
}

TEST(Geometry, TreatsTouchingAabbsAsIntersecting)
{
    const hw3d::Aabb left{{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}};
    const hw3d::Aabb touching{{1.0F, 0.5F, 0.5F}, {2.0F, 2.0F, 2.0F}};
    const hw3d::Aabb separate{{1.1F, 0.5F, 0.5F}, {2.0F, 2.0F, 2.0F}};

    EXPECT_TRUE(hw3d::aabbs_intersect(left, touching));
    EXPECT_FALSE(hw3d::aabbs_intersect(left, separate));
}

TEST(Geometry, BuildsSharedVoxelGridAndAssignsAabbCenters)
{
    const std::vector<hw3d::Aabb> bounds{
        {{-2.0F, 1.0F, 5.0F}, {2.0F, 3.0F, 5.0F}},
        {{6.0F, -1.0F, 4.0F}, {8.0F, 5.0F, 10.0F}},
    };

    const hw3d::VoxelGrid grid = hw3d::make_voxel_grid(bounds);

    EXPECT_FLOAT_EQ(grid.origin.x, -2.0F);
    EXPECT_FLOAT_EQ(grid.origin.y, -1.0F);
    EXPECT_FLOAT_EQ(grid.origin.z, 4.0F);
    EXPECT_FLOAT_EQ(grid.voxel_size.x, 4.0F);
    EXPECT_FLOAT_EQ(grid.voxel_size.y, 6.0F);
    EXPECT_FLOAT_EQ(grid.voxel_size.z, 6.0F);
    EXPECT_EQ(hw3d::voxel_for(hw3d::center(bounds[0]), grid),
              (hw3d::Voxel{0, 0, 0}));
    EXPECT_EQ(hw3d::voxel_for(hw3d::center(bounds[1]), grid),
              (hw3d::Voxel{2, 0, 0}));
}

TEST(Geometry, UsesPositiveVoxelSizeForFlatGeometry)
{
    const std::vector<hw3d::Aabb> bounds{
        {{1.0F, 2.0F, 3.0F}, {1.0F, 2.0F, 3.0F}},
    };

    const hw3d::VoxelGrid grid = hw3d::make_voxel_grid(bounds);

    EXPECT_FLOAT_EQ(grid.voxel_size.x, 1.0F);
    EXPECT_FLOAT_EQ(grid.voxel_size.y, 1.0F);
    EXPECT_FLOAT_EQ(grid.voxel_size.z, 1.0F);
    EXPECT_EQ(hw3d::voxel_for(hw3d::center(bounds[0]), grid),
              (hw3d::Voxel{0, 0, 0}));
}

TEST(Geometry, BuildsNoMotionPairsForEmptyAndSingleInputs)
{
    const std::vector<hw3d::Aabb> empty;
    const auto empty_candidates =
        hw3d::make_motion_pair_candidates(empty, 10);
    ASSERT_TRUE(empty_candidates.pairs.has_value());
    EXPECT_TRUE(empty_candidates.pairs->empty());
    EXPECT_TRUE(empty_candidates.voxels.empty());

    const std::vector<hw3d::Aabb> single{
        {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}},
    };
    const auto single_candidates =
        hw3d::make_motion_pair_candidates(single, 10);
    ASSERT_TRUE(single_candidates.pairs.has_value());
    EXPECT_TRUE(single_candidates.pairs->empty());
    EXPECT_EQ(single_candidates.voxels.size(), 1U);
}

TEST(Geometry, BuildsUniqueInclusiveMotionPairs)
{
    const std::vector<hw3d::Aabb> bounds{
        {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}},
        {{0.5F, 0.0F, 0.0F}, {1.5F, 1.0F, 1.0F}},
        {{1.5F, 0.0F, 0.0F}, {2.5F, 1.0F, 1.0F}},
        {{1'000'000.0F, 0.0F, 0.0F},
         {1'000'001.0F, 1.0F, 1.0F}},
    };

    const auto candidates = hw3d::make_motion_pair_candidates(bounds, 10);

    ASSERT_TRUE(candidates.pairs.has_value());
    ASSERT_EQ(candidates.pairs->size(), 2U);
    EXPECT_EQ((*candidates.pairs)[0], (hw3d::TrianglePair{0, 1}));
    EXPECT_EQ((*candidates.pairs)[1], (hw3d::TrianglePair{1, 2}));
}

TEST(Geometry, RetainsVoxelsWhenMotionPairBudgetIsExceeded)
{
    const hw3d::Aabb shared{
        {0.0F, 0.0F, 0.0F},
        {1.0F, 1.0F, 1.0F},
    };
    const std::vector bounds{shared, shared, shared};

    const auto candidates = hw3d::make_motion_pair_candidates(bounds, 1);

    EXPECT_FALSE(candidates.pairs.has_value());
    ASSERT_EQ(candidates.voxels.size(), bounds.size());
    EXPECT_EQ(candidates.voxels[0], candidates.voxels[1]);
    EXPECT_EQ(candidates.voxels[1], candidates.voxels[2]);

    std::vector<hw3d::Aabb> invalid{shared};
    invalid[0].minimum.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        static_cast<void>(hw3d::make_motion_pair_candidates(invalid, 1)),
        std::invalid_argument);
}

TEST(Geometry, ZeroPairBudgetAllowsEmptyCacheButFallsBackForFirstPair)
{
    const std::vector<hw3d::Aabb> disjoint{
        {{0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}},
        {{2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 1.0F}},
    };
    const auto empty = hw3d::make_motion_pair_candidates(disjoint, 0);
    ASSERT_TRUE(empty.pairs.has_value());
    EXPECT_TRUE(empty.pairs->empty());

    const std::vector<hw3d::Aabb> touching{
        disjoint[0],
        {{1.0F, 0.0F, 0.0F}, {2.0F, 1.0F, 1.0F}},
    };
    const auto fallback = hw3d::make_motion_pair_candidates(touching, 0);
    EXPECT_FALSE(fallback.pairs.has_value());
    EXPECT_EQ(fallback.voxels.size(), touching.size());
}

TEST(Geometry, AcceptsExactlyTheConfiguredPairLimit)
{
    EXPECT_EQ(
        hw3d::maximum_cached_triangle_pair_count,
        256'000'000U);

    const hw3d::Aabb shared{
        {0.0F, 0.0F, 0.0F},
        {1.0F, 1.0F, 1.0F},
    };
    const std::vector bounds{shared, shared, shared};

    const auto exact = hw3d::make_motion_pair_candidates(bounds, 3);
    ASSERT_TRUE(exact.pairs.has_value());
    EXPECT_EQ(exact.pairs->size(), 3U);

    const auto too_small = hw3d::make_motion_pair_candidates(bounds, 2);
    EXPECT_FALSE(too_small.pairs.has_value());
}

TEST(Geometry, IncludesFaceEdgeAndVertexAabbContacts)
{
    const hw3d::Aabb base{
        {0.0F, 0.0F, 0.0F},
        {1.0F, 1.0F, 1.0F},
    };
    const std::vector<hw3d::Aabb> contacts{
        {{1.0F, 0.25F, 0.25F}, {2.0F, 0.75F, 0.75F}},
        {{1.0F, 1.0F, 0.25F}, {2.0F, 2.0F, 0.75F}},
        {{1.0F, 1.0F, 1.0F}, {2.0F, 2.0F, 2.0F}},
    };

    for (const hw3d::Aabb& contact : contacts) {
        const std::vector bounds{base, contact};
        const auto candidates =
            hw3d::make_motion_pair_candidates(bounds, 1);
        ASSERT_TRUE(candidates.pairs.has_value());
        ASSERT_EQ(candidates.pairs->size(), 1U);
        EXPECT_EQ(candidates.pairs->front(), (hw3d::TrianglePair{0, 1}));
    }
}

TEST(Geometry, HandlesZeroExtentMotionBounds)
{
    const std::vector<hw3d::Aabb> bounds{
        {{4.0F, -2.0F, 7.0F}, {4.0F, -2.0F, 7.0F}},
        {{4.0F, -2.0F, 7.0F}, {4.0F, -2.0F, 7.0F}},
        {{4.0F, -2.0F, 8.0F}, {4.0F, -2.0F, 8.0F}},
    };

    const auto candidates = hw3d::make_motion_pair_candidates(bounds, 3);

    ASSERT_TRUE(candidates.pairs.has_value());
    ASSERT_EQ(candidates.pairs->size(), 1U);
    EXPECT_EQ(candidates.pairs->front(), (hw3d::TrianglePair{0, 1}));
}

TEST(Geometry, ProducesEveryDensePairExactlyOnce)
{
    const hw3d::Aabb shared{
        {-1.0F, -1.0F, -1.0F},
        {1.0F, 1.0F, 1.0F},
    };
    const std::vector<hw3d::Aabb> bounds(5, shared);

    const auto candidates = hw3d::make_motion_pair_candidates(bounds, 10);

    ASSERT_TRUE(candidates.pairs.has_value());
    ASSERT_EQ(candidates.pairs->size(), 10U);
    for (std::size_t index = 0; index < candidates.pairs->size(); ++index) {
        const hw3d::TrianglePair pair = (*candidates.pairs)[index];
        EXPECT_LT(pair.first, pair.second);
        for (std::size_t other = index + 1;
             other < candidates.pairs->size();
             ++other) {
            EXPECT_FALSE(pair == (*candidates.pairs)[other]);
        }
    }
}

TEST(Geometry, RejectsInfiniteAndReversedMotionBounds)
{
    hw3d::Aabb invalid{
        {0.0F, 0.0F, 0.0F},
        {1.0F, 1.0F, 1.0F},
    };
    invalid.maximum.z = std::numeric_limits<float>::infinity();
    EXPECT_THROW(
        static_cast<void>(hw3d::make_motion_pair_candidates(
            std::vector{invalid}, 1)),
        std::invalid_argument);

    invalid = {{2.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}};
    EXPECT_THROW(
        static_cast<void>(hw3d::make_motion_pair_candidates(
            std::vector{invalid}, 1)),
        std::invalid_argument);
}

TEST(Geometry, IdentifiesDegenerateTriangles)
{
    const hw3d::Triangle proper{
        {0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
    };
    const hw3d::Triangle point{
        {1.0F, 2.0F, 3.0F},
        {1.0F, 2.0F, 3.0F},
        {1.0F, 2.0F, 3.0F},
    };
    const hw3d::Triangle segment{
        {-1.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {-1.0F, 0.0F, 0.0F},
    };

    EXPECT_FALSE(hw3d::is_degenerate(proper));
    EXPECT_TRUE(hw3d::is_degenerate(point));
    EXPECT_TRUE(hw3d::is_degenerate(segment));
}

TEST(Geometry, DegeneracyIsInvariantForTranslationAndScale)
{
    const hw3d::Triangle proper{
        {0.0F, 0.0F, 0.0F},
        {2.0F, 0.0F, 0.0F},
        {0.0F, 2.0F, 0.0F},
    };
    const hw3d::Triangle small{
        {0.0F, 0.0F, 0.0F},
        {2.0e-9F, 0.0F, 0.0F},
        {0.0F, 2.0e-9F, 0.0F},
    };
    const hw3d::Triangle translated{
        {1'000'000.0F, -1'000'000.0F, 500'000.0F},
        {1'000'002.0F, -1'000'000.0F, 500'000.0F},
        {1'000'000.0F, -999'998.0F, 500'000.0F},
    };

    EXPECT_FALSE(hw3d::is_degenerate(proper));
    EXPECT_FALSE(hw3d::is_degenerate(small));
    EXPECT_FALSE(hw3d::is_degenerate(translated));
}

}
