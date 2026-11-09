#include <gtest/gtest.h>

#include <vector>

import hw3d.configuration;
import hw3d.geometry;

TEST(Geometry, ProducesFullRotationBounds)
{
    const hw3d::InitialTriangle triangle{
        .a = {2.0F, 0.0F, 0.0F},
        .b = {0.0F, 2.0F, 0.0F},
        .c = {0.0F, 0.0F, 2.0F},
        .rotation_axis_origin = {0.0F, 0.0F, 0.0F},
        .rotation_axis_direction = {0.0F, 0.0F, 1.0F},
        .angular_velocity_radians_per_second = 1.0F,
    };
    const hw3d::Aabb bounds = hw3d::motion_aabb(triangle);
    EXPECT_LE(bounds.minimum.x, -2.0F);
    EXPECT_LE(bounds.minimum.y, -2.0F);
    EXPECT_GE(bounds.maximum.x, 2.0F);
    EXPECT_GE(bounds.maximum.y, 2.0F);
}

TEST(Geometry, PairCacheAndVoxelFallbackPreserveIds)
{
    const std::vector<hw3d::Aabb> bounds{
        {{0, 0, 0}, {2, 2, 0}},
        {{1, 1, 0}, {3, 3, 0}},
        {{20, 20, 0}, {21, 21, 0}},
    };
    const auto cached = hw3d::make_motion_pair_candidates(bounds, 16);
    ASSERT_TRUE(cached.pairs.has_value());
    ASSERT_EQ(cached.pairs->size(), 1U);
    EXPECT_EQ((*cached.pairs)[0], (hw3d::TrianglePair{0, 1}));

    const auto fallback = hw3d::make_motion_pair_candidates(bounds, 0);
    EXPECT_FALSE(fallback.pairs.has_value());
    EXPECT_EQ(fallback.voxels.size(), bounds.size());
}
