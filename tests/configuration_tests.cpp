#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>

import hw3d.configuration;

namespace {

std::string read_error(const std::string& input)
{
    std::istringstream stream{input};

    try {
        static_cast<void>(hw3d::read_configuration(stream));
    } catch (const std::runtime_error& error) {
        return error.what();
    }

    return {};
}

TEST(Configuration, ReadsSimulationAndOneRotatingTriangle)
{
    std::istringstream input{
        "1\n"
        "12\n"
        "-1.5 2.25 3 4 5.5 6 7 8 9.75\n"
        "10 20 30 10 23 34\n"
        "90\n"};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);

    EXPECT_EQ(configuration.simulation_duration, std::chrono::seconds{12});
    ASSERT_EQ(configuration.triangles.size(), 1U);
    const hw3d::InitialTriangle& triangle = configuration.triangles.front();
    EXPECT_FLOAT_EQ(triangle.a.x, -1.5F);
    EXPECT_FLOAT_EQ(triangle.a.y, 2.25F);
    EXPECT_FLOAT_EQ(triangle.a.z, 3.0F);
    EXPECT_FLOAT_EQ(triangle.b.x, 4.0F);
    EXPECT_FLOAT_EQ(triangle.b.y, 5.5F);
    EXPECT_FLOAT_EQ(triangle.b.z, 6.0F);
    EXPECT_FLOAT_EQ(triangle.c.x, 7.0F);
    EXPECT_FLOAT_EQ(triangle.c.y, 8.0F);
    EXPECT_FLOAT_EQ(triangle.c.z, 9.75F);
    EXPECT_FLOAT_EQ(triangle.rotation_axis_origin.x, 10.0F);
    EXPECT_FLOAT_EQ(triangle.rotation_axis_origin.y, 20.0F);
    EXPECT_FLOAT_EQ(triangle.rotation_axis_origin.z, 30.0F);
    EXPECT_FLOAT_EQ(triangle.rotation_axis_direction.x, 0.0F);
    EXPECT_FLOAT_EQ(triangle.rotation_axis_direction.y, 0.6F);
    EXPECT_FLOAT_EQ(triangle.rotation_axis_direction.z, 0.8F);
    EXPECT_FLOAT_EQ(
        triangle.angular_velocity_radians_per_second,
        -std::numbers::pi_v<float> / 2.0F);
}

TEST(Configuration, AcceptsArbitraryWhitespaceAndMultipleTriangles)
{
    std::istringstream input{
        "  2\n\n 5\t"
        "0 0 0\t1 0 0\n0 1 0\n"
        "0 0 0  1 0 0  0\n"
        "2 2 2  3 2 2  2 3 2\n"
        "4 5 6\n4 5 8\n180   "};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);

    EXPECT_EQ(configuration.simulation_duration, std::chrono::seconds{5});
    ASSERT_EQ(configuration.triangles.size(), 2U);
    EXPECT_FLOAT_EQ(configuration.triangles[0].c.y, 1.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[0].angular_velocity_radians_per_second, 0.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[1].a.x, 2.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[1].c.y, 3.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[1].rotation_axis_direction.z, 1.0F);
    EXPECT_FLOAT_EQ(
        configuration.triangles[1].angular_velocity_radians_per_second,
        -std::numbers::pi_v<float>);
}

TEST(Configuration, LeavesTrailingInputUnread)
{
    std::istringstream input{
        "1 1 0 0 0 1 0 0 0 1 0 0 0 0 0 0 1 0 trailing-data"};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);
    std::string trailing;
    input >> trailing;

    ASSERT_EQ(configuration.triangles.size(), 1U);
    EXPECT_EQ(trailing, "trailing-data");
}

TEST(Configuration, PreservesDegenerateTriangle)
{
    std::istringstream input{
        "1 1 4 4 4 4 4 4 4 4 4 0 0 0 0 1 0 0"};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);

    ASSERT_EQ(configuration.triangles.size(), 1U);
    EXPECT_FLOAT_EQ(configuration.triangles[0].a.x, 4.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[0].b.y, 4.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[0].c.z, 4.0F);
}

TEST(Configuration, RejectsMissingTriangleCount)
{
    EXPECT_EQ(read_error(""), "failed to read triangle count");
}

TEST(Configuration, RejectsNonNumericTriangleCount)
{
    EXPECT_EQ(read_error("triangle"), "failed to read triangle count");
}

TEST(Configuration, RejectsNonPositiveTriangleCounts)
{
    EXPECT_EQ(
        read_error("0"),
        "triangle count must be in the range (0, 1000000)");
    EXPECT_EQ(
        read_error("-1"),
        "triangle count must be in the range (0, 1000000)");
}

TEST(Configuration, RejectsUpperBoundaryAndLargerCounts)
{
    EXPECT_EQ(
        read_error("1000000"),
        "triangle count must be in the range (0, 1000000)");
    EXPECT_EQ(
        read_error("1000001"),
        "triangle count must be in the range (0, 1000000)");
}

TEST(Configuration, RejectsMissingAndNonIntegralSimulationDuration)
{
    EXPECT_EQ(
        read_error("1"),
        "failed to read simulation duration");
    EXPECT_EQ(
        read_error("1 forever"),
        "failed to read simulation duration");
    EXPECT_EQ(
        read_error("1 1.5"),
        "failed to read simulation duration");
}

TEST(Configuration, RejectsNonPositiveSimulationDuration)
{
    EXPECT_EQ(
        read_error("1 0"),
        "simulation duration must be positive");
    EXPECT_EQ(
        read_error("1 -1"),
        "simulation duration must be positive");
}

TEST(Configuration, ReportsTheFirstIncompleteTriangle)
{
    EXPECT_EQ(
        read_error("1 1 0 0 0 1 0 0 0 1"),
        "failed to read triangle 1");
}

TEST(Configuration, ReportsLaterIncompleteTriangleWithOneBasedIndex)
{
    EXPECT_EQ(
        read_error(
            "2 1 "
            "0 0 0 1 0 0 0 1 0 0 0 0 0 0 1 0 "
            "0 0 0 1 0 invalid"),
        "failed to read triangle 2");
}

TEST(Configuration, ReportsIncompleteRotationAxis)
{
    EXPECT_EQ(
        read_error("1 1 0 0 0 1 0 0 0 1 0 0 0"),
        "failed to read rotation axis for triangle 1");
    EXPECT_EQ(
        read_error("1 1 0 0 0 1 0 0 0 1 0 0 0 invalid"),
        "failed to read rotation axis for triangle 1");
}

TEST(Configuration, RejectsCoincidentRotationAxisPoints)
{
    EXPECT_EQ(
        read_error("1 1 0 0 0 1 0 0 0 1 0 2 3 4 2 3 4 10"),
        "rotation axis for triangle 1 must use distinct finite points");
}

TEST(Configuration, ReportsMissingAngularSpeed)
{
    EXPECT_EQ(
        read_error("1 1 0 0 0 1 0 0 0 1 0 0 0 0 0 0 1"),
        "failed to read angular speed for triangle 1");
    EXPECT_EQ(
        read_error("1 1 0 0 0 1 0 0 0 1 0 0 0 0 0 0 1 fast"),
        "failed to read angular speed for triangle 1");
}

TEST(Configuration, ReadsNegativeAngularSpeedAsCounterclockwise)
{
    std::istringstream input{
        "1 1 0 0 0 1 0 0 0 1 0 0 0 0 0 0 1 -90"};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);

    ASSERT_EQ(configuration.triangles.size(), 1U);
    EXPECT_FLOAT_EQ(
        configuration.triangles[0]
            .angular_velocity_radians_per_second,
        std::numbers::pi_v<float> / 2.0F);
}

TEST(Configuration, ConvertsMaximumFiniteAngularSpeedsWithoutOverflow)
{
    const auto read_speed = [](const float speed) {
        std::ostringstream text;
        text.precision(std::numeric_limits<float>::max_digits10);
        text << "1 1 0 0 0 1 0 0 0 1 0 0 0 0 0 0 1 " << speed;
        std::istringstream input{text.str()};
        return hw3d::read_configuration(input)
            .triangles.front().angular_velocity_radians_per_second;
    };
    constexpr float factor = std::numbers::pi_v<float> / 180.0F;
    const float positive_input = std::numeric_limits<float>::max();
    const float clockwise = read_speed(positive_input);
    const float counterclockwise = read_speed(-positive_input);

    EXPECT_TRUE(std::isfinite(clockwise));
    EXPECT_TRUE(std::isfinite(counterclockwise));
    EXPECT_FLOAT_EQ(clockwise, -positive_input * factor);
    EXPECT_FLOAT_EQ(counterclockwise, positive_input * factor);
}

TEST(Configuration, PreservesPositiveZeroAngularSpeed)
{
    std::istringstream input{
        "1 1 0 0 0 1 0 0 0 1 0 0 0 0 0 0 1 -0"};

    const float speed = hw3d::read_configuration(input)
        .triangles.front().angular_velocity_radians_per_second;

    EXPECT_EQ(speed, 0.0F);
    EXPECT_FALSE(std::signbit(speed));
}

}
