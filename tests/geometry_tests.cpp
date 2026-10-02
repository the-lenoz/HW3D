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
