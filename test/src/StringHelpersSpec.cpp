#include "Baldr/StringHelpers.hpp"

class StringHelpersTests : public ::testing::Test
{};

TEST_F(StringHelpersTests, DecodePathShouldRejectNullBytes)
{
    auto result = decode_path("/etc/passwd%00.txt");

    ASSERT_FALSE(result.has_value());
}

TEST_F(StringHelpersTests, DecodePathShouldRejectTruncatedEncoding)
{
    auto result = decode_path("/path%");

    ASSERT_FALSE(result.has_value());
}

TEST_F(StringHelpersTests, DecodePathShouldRejectInvalidHex)
{
    auto result = decode_path("/path%ZZ");

    ASSERT_FALSE(result.has_value());
}

TEST_F(StringHelpersTests, DecodePathShouldDecodeValidEncoding)
{
    auto result = decode_path("/hello%20world");

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), "/hello world");
}

TEST_F(StringHelpersTests, TrimShouldRemoveWhitespace)
{
    ASSERT_EQ(trim("  hello  "), "hello");
    ASSERT_EQ(trim("\t\nhello\t\n"), "hello");
    ASSERT_EQ(trim("  "), "");
    ASSERT_EQ(trim(""), "");
}
