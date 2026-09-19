#include <gtest/gtest.h>

#include <glad/gl.h>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <vector>

#include "embedded_shaders.hpp"

import hw3d.configuration;
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
        const std::vector<Triangle>& triangles,
        const std::initializer_list<bool> expected)
    {
        const std::vector<bool> actual = hw3d::gpu_collisions(triangles);

        ASSERT_EQ(actual.size(), expected.size());
        std::size_t index = 0;
        for (const bool value : expected) {
            EXPECT_EQ(actual[index], value)
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

TEST_F(GpuIntersectionsTest, DetectsIdenticalTriangles)
{
    expect_collisions({base_triangle, base_triangle}, {true, true});
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

TEST_F(GpuIntersectionsTest, DispatchesMoreThanOneWorkGroup)
{
    constexpr std::size_t triangle_count = 130;
    std::vector<Triangle> triangles;
    triangles.reserve(triangle_count);

    for (std::size_t index = 0; index < triangle_count; ++index) {
        const float x = static_cast<float>(index) * 4.0F;
        triangles.push_back(triangle(
            {x, 0.0F, 0.0F},
            {x + 1.0F, 0.0F, 0.0F},
            {x, 1.0F, 0.0F}));
    }

    const std::vector<bool> result = hw3d::gpu_collisions(triangles);

    ASSERT_EQ(result.size(), triangle_count);
    for (std::size_t index = 0; index < result.size(); ++index) {
        EXPECT_FALSE(result[index]) << "triangle index: " << index;
    }
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuIntersectionsTest, CurrentlyIgnoresDegenerateTriangles)
{
    const Triangle inside = point({0.5F, 0.5F, 0.0F});

    expect_collisions({base_triangle, inside}, {false, false});
}

}
