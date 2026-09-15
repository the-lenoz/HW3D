#include <gtest/gtest.h>

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

TEST(Configuration, ReadsOneTriangle)
{
    std::istringstream input{
        "1\n"
        "-1.5 2.25 3 4 5.5 6 7 8 9.75\n"};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);

    ASSERT_EQ(configuration.triangles.size(), 1U);
    const hw3d::Triangle& triangle = configuration.triangles.front();
    EXPECT_FLOAT_EQ(triangle.a.x, -1.5F);
    EXPECT_FLOAT_EQ(triangle.a.y, 2.25F);
    EXPECT_FLOAT_EQ(triangle.a.z, 3.0F);
    EXPECT_FLOAT_EQ(triangle.b.x, 4.0F);
    EXPECT_FLOAT_EQ(triangle.b.y, 5.5F);
    EXPECT_FLOAT_EQ(triangle.b.z, 6.0F);
    EXPECT_FLOAT_EQ(triangle.c.x, 7.0F);
    EXPECT_FLOAT_EQ(triangle.c.y, 8.0F);
    EXPECT_FLOAT_EQ(triangle.c.z, 9.75F);
}

TEST(Configuration, AcceptsArbitraryWhitespaceAndMultipleTriangles)
{
    std::istringstream input{
        "  2\n\n"
        "0 0 0\t1 0 0\n0 1 0\n"
        "2 2 2  3 2 2  2 3 2   "};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);

    ASSERT_EQ(configuration.triangles.size(), 2U);
    EXPECT_FLOAT_EQ(configuration.triangles[0].c.y, 1.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[1].a.x, 2.0F);
    EXPECT_FLOAT_EQ(configuration.triangles[1].c.y, 3.0F);
}

TEST(Configuration, LeavesTrailingInputUnread)
{
    std::istringstream input{
        "1 0 0 0 1 0 0 0 1 0 trailing-data"};

    const hw3d::Configuration configuration =
        hw3d::read_configuration(input);
    std::string trailing;
    input >> trailing;

    ASSERT_EQ(configuration.triangles.size(), 1U);
    EXPECT_EQ(trailing, "trailing-data");
}

TEST(Configuration, PreservesDegenerateTriangle)
{
    std::istringstream input{"1 4 4 4 4 4 4 4 4 4"};

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

TEST(Configuration, ReportsTheFirstIncompleteTriangle)
{
    EXPECT_EQ(
        read_error("1 0 0 0 1 0 0 0 1"),
        "failed to read triangle 1");
}

TEST(Configuration, ReportsLaterIncompleteTriangleWithOneBasedIndex)
{
    EXPECT_EQ(
        read_error(
            "2 "
            "0 0 0 1 0 0 0 1 0 "
            "0 0 0 1 0 invalid"),
        "failed to read triangle 2");
}

}
