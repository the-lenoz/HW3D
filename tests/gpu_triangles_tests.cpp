#include <gtest/gtest.h>

#include <glad/gl.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>

import hw3d.configuration;
import hw3d.geometry;
import hw3d.gpu_triangles;
import hw3d.window_context;

namespace {

void clear_gl_errors() noexcept
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

void expect_vec3_near(
    const hw3d::Vec3& actual,
    const hw3d::Vec3& expected,
    const float tolerance = 2.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

void expect_triangle_near(
    const hw3d::Triangle& actual,
    const hw3d::Triangle& expected)
{
    expect_vec3_near(actual.a, expected.a);
    expect_vec3_near(actual.b, expected.b);
    expect_vec3_near(actual.c, expected.c);
    EXPECT_EQ(actual.highlighted, expected.highlighted);
}

class GpuTrianglesTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        window_ = std::make_unique<hw3d::WindowContext>(
            64,
            64,
            "GPU triangle tests");
    }

    static void TearDownTestSuite()
    {
        window_.reset();
    }

    void SetUp() override
    {
        clear_gl_errors();
    }

    static inline std::unique_ptr<hw3d::WindowContext> window_;
};

TEST_F(GpuTrianglesTest, UploadDownloadRoundTripPreservesRuntimeState)
{
    const std::vector<hw3d::Triangle> input{
        {
            .a = {-3.0F, 2.0F, 1.0F},
            .b = {4.0F, -5.0F, 6.0F},
            .c = {7.0F, 8.0F, -9.0F},
            .highlighted = true,
        },
        {
            .a = {0.0F, 0.0F, 0.0F},
            .b = {1.0F, 0.0F, 0.0F},
            .c = {0.0F, 1.0F, 0.0F},
        },
    };
    std::vector<hw3d::Triangle> output(input.size());
    hw3d::GpuTriangleBuffer buffer{input.size()};

    buffer.upload(input);
    buffer.download(output);

    ASSERT_EQ(output.size(), input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        EXPECT_FLOAT_EQ(output[index].a.x, input[index].a.x);
        EXPECT_FLOAT_EQ(output[index].a.y, input[index].a.y);
        EXPECT_FLOAT_EQ(output[index].a.z, input[index].a.z);
        EXPECT_FLOAT_EQ(output[index].b.x, input[index].b.x);
        EXPECT_FLOAT_EQ(output[index].b.y, input[index].b.y);
        EXPECT_FLOAT_EQ(output[index].b.z, input[index].b.z);
        EXPECT_FLOAT_EQ(output[index].c.x, input[index].c.x);
        EXPECT_FLOAT_EQ(output[index].c.y, input[index].c.y);
        EXPECT_FLOAT_EQ(output[index].c.z, input[index].c.z);
        EXPECT_EQ(output[index].highlighted, input[index].highlighted);
    }
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuTrianglesTest, RejectsTransferSizeMismatchAndNonFiniteInput)
{
    hw3d::GpuTriangleBuffer buffer{1};
    std::vector<hw3d::Triangle> empty;
    std::vector<hw3d::Triangle> invalid(1);
    invalid[0].a.x = std::numeric_limits<float>::quiet_NaN();

    EXPECT_THROW(buffer.upload(empty), std::invalid_argument);
    EXPECT_THROW(buffer.download(empty), std::invalid_argument);
    EXPECT_THROW(buffer.upload(invalid), std::invalid_argument);
}

TEST_F(GpuTrianglesTest, ZeroTimeAndZeroSpeedCopyOriginalFloatsExactly)
{
    const std::vector<hw3d::InitialTriangle> initial{
        {
            .a = {-3.25F, 2.5F, 1.75F},
            .b = {4.5F, -5.25F, 6.0F},
            .c = {7.0F, 8.0F, -9.5F},
            .rotation_axis_origin = {100.0F, 200.0F, 300.0F},
            .rotation_axis_direction = {2.0F, 3.0F, 4.0F},
            .angular_velocity_radians_per_second = 0.0F,
        },
    };
    hw3d::GpuTriangleBuffer buffer{initial.size()};
    hw3d::GpuVertexUpdater updater{initial, buffer};
    std::vector<hw3d::Triangle> output(initial.size());

    updater.update(std::chrono::duration<double>::zero());
    buffer.download(output);

    EXPECT_FLOAT_EQ(output[0].a.x, initial[0].a.x);
    EXPECT_FLOAT_EQ(output[0].a.y, initial[0].a.y);
    EXPECT_FLOAT_EQ(output[0].a.z, initial[0].a.z);
    EXPECT_FLOAT_EQ(output[0].b.x, initial[0].b.x);
    EXPECT_FLOAT_EQ(output[0].b.y, initial[0].b.y);
    EXPECT_FLOAT_EQ(output[0].b.z, initial[0].b.z);
    EXPECT_FLOAT_EQ(output[0].c.x, initial[0].c.x);
    EXPECT_FLOAT_EQ(output[0].c.y, initial[0].c.y);
    EXPECT_FLOAT_EQ(output[0].c.z, initial[0].c.z);
    EXPECT_FALSE(output[0].highlighted);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuTrianglesTest, RotationMatchesCpuForOffsetScaledAxis)
{
    const std::vector<hw3d::InitialTriangle> initial{
        {
            .a = {9.0F, -2.0F, 4.0F},
            .b = {7.0F, 3.0F, -1.0F},
            .c = {2.0F, 5.0F, 8.0F},
            .rotation_axis_origin = {3.0F, -4.0F, 2.0F},
            .rotation_axis_direction = {2.0F, -3.0F, 5.0F},
            .angular_velocity_radians_per_second = 1.25F,
        },
    };
    constexpr std::chrono::duration<double> elapsed{0.73};
    std::vector<hw3d::Triangle> expected(initial.size());
    std::vector<hw3d::Triangle> actual(initial.size());
    hw3d::update_vertices(initial, expected, elapsed);
    hw3d::GpuTriangleBuffer buffer{initial.size()};
    hw3d::GpuVertexUpdater updater{initial, buffer};

    updater.update(elapsed);
    buffer.download(actual);

    expect_triangle_near(actual[0], expected[0]);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuTrianglesTest, UpdatesFromAbsoluteTimeWithoutAccumulation)
{
    const std::vector<hw3d::InitialTriangle> initial{
        {
            .a = {2.0F, 0.0F, 0.0F},
            .b = {0.0F, 2.0F, 0.0F},
            .c = {0.0F, 0.0F, 2.0F},
            .rotation_axis_origin = {0.5F, -0.5F, 0.25F},
            .rotation_axis_direction = {0.0F, 0.0F, 7.0F},
            .angular_velocity_radians_per_second =
                std::numbers::pi_v<float> * 0.5F,
        },
    };
    hw3d::GpuTriangleBuffer buffer{initial.size()};
    hw3d::GpuVertexUpdater updater{initial, buffer};
    std::vector<hw3d::Triangle> expected(initial.size());
    std::vector<hw3d::Triangle> actual(initial.size());

    updater.update(std::chrono::duration<double>{1.5});
    updater.update(std::chrono::duration<double>{0.25});
    buffer.download(actual);
    hw3d::update_vertices(
        initial,
        expected,
        std::chrono::duration<double>{0.25});

    expect_triangle_near(actual[0], expected[0]);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(GpuTrianglesTest, ValidatesUpdaterShapeAndElapsedTime)
{
    const std::vector<hw3d::InitialTriangle> initial(1);
    hw3d::GpuTriangleBuffer wrong_size{2};
    EXPECT_THROW(
        hw3d::GpuVertexUpdater(initial, wrong_size),
        std::invalid_argument);

    hw3d::GpuTriangleBuffer buffer{initial.size()};
    hw3d::GpuVertexUpdater updater{initial, buffer};
    EXPECT_THROW(
        updater.update(std::chrono::duration<double>{-1.0}),
        std::invalid_argument);
    EXPECT_THROW(
        updater.update(std::chrono::duration<double>{
            std::numeric_limits<double>::infinity()}),
        std::invalid_argument);
}

TEST_F(GpuTrianglesTest, EmptySceneCanBeUpdatedAndTransferred)
{
    const std::vector<hw3d::InitialTriangle> initial;
    std::vector<hw3d::Triangle> current;
    hw3d::GpuTriangleBuffer buffer{0};
    hw3d::GpuVertexUpdater updater{initial, buffer};

    EXPECT_NO_THROW(updater.update(std::chrono::duration<double>::zero()));
    EXPECT_NO_THROW(buffer.upload(current));
    EXPECT_NO_THROW(buffer.download(current));
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

}
