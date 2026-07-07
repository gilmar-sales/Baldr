#include <Baldr/Http/FromParams.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
    struct UserPathArgs
    {
        std::string id;
        int         age    = 0;
        bool        active = false;
    };
} // namespace

TEST(FromParamsTest, ParsesValidPathParametersIntoShell)
{
    baldr::HttpRequest request;
    request.params["id"]     = std::string("u-1");
    request.params["age"]    = std::string("30");
    request.params["active"] = std::string("true");

    auto bound = baldr::detail::bindFromParams<UserPathArgs>(request);

    EXPECT_TRUE(bound.isOk());
    EXPECT_FALSE(bound.error.has_value());
    EXPECT_EQ(bound.value.id, "u-1");
    EXPECT_EQ(bound.value.age, 30);
    EXPECT_TRUE(bound.value.active);
}

TEST(FromParamsTest, ReportsFieldMissingFromPathTemplate)
{
    baldr::HttpRequest request;
    request.params["id"] = std::string("u-1");
    // "age" + "active" missing

    auto bound = baldr::detail::bindFromParams<UserPathArgs>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_EQ(static_cast<int>(bound.error->statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_NE(bound.error->message.find("path template"), std::string::npos);
}

TEST(FromParamsTest, ReportsNumericParseFailure)
{
    baldr::HttpRequest request;
    request.params["id"]     = std::string("u-1");
    request.params["age"]    = std::string("thirty");
    request.params["active"] = std::string("true");

    auto bound = baldr::detail::bindFromParams<UserPathArgs>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_NE(bound.error->message.find("could not be parsed"),
              std::string::npos);
}

TEST(FromParamsTest, IsFromParamsTraitDetectsWrapper)
{
    static_assert(baldr::isFromParams_v<baldr::FromParams<UserPathArgs>>,
                  "isFromParams_v must recognise FromParams<U>");
    static_assert(!baldr::isFromParams_v<UserPathArgs>,
                  "isFromParams_v must reject non-wrapper types");
    static_assert(!baldr::isFromParams_v<int>,
                  "isFromParams_v must reject primitives");
    static_assert(
        std::is_same_v<
            baldr::isFromParams<baldr::FromParams<UserPathArgs>>::ValueType,
            UserPathArgs>,
        "ValueType alias must surface the wrapped payload type");
}
