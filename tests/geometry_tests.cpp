#include <gtest/gtest.h>

#include <vector>

import hw3d.configuration;
import hw3d.geometry;

namespace {

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

}
