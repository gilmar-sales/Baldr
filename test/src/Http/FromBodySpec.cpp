#include <Baldr/Http/FromBody.hpp>

#include "UserDto.hpp"

TEST(FromBodyTest, ParsesValidJsonObjectIntoShell)
{
    baldr::HttpRequest request;
    request.body = R"({"name":"Alice","age":30})";

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_TRUE(bound.isOk());
    EXPECT_FALSE(bound.error.has_value());
    EXPECT_EQ(bound.value.name, "Alice");
    EXPECT_EQ(bound.value.age, 30);
    EXPECT_EQ(bound.Value().name, "Alice");
}

TEST(FromBodyTest, ReportsFieldMismatch)
{
    baldr::HttpRequest request;
    request.body = R"({"name":"Bob"})"; // missing "age"

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_EQ(static_cast<int>(bound.error->statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_FALSE(bound.error->message.empty());
}

TEST(FromBodyTest, RejectsEmptyBody)
{
    baldr::HttpRequest request;
    request.body = "";

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_EQ(static_cast<int>(bound.error->statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_NE(bound.error->message.find("Empty"), std::string::npos);
}

TEST(FromBodyTest, RejectsMalformedJson)
{
    baldr::HttpRequest request;
    request.body = "not-json";

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_EQ(static_cast<int>(bound.error->statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_NE(bound.error->message.find("Invalid JSON"), std::string::npos);
}

TEST(FromBodyTest, RejectsTopLevelNonObject)
{
    baldr::HttpRequest request;
    request.body = "[1,2,3]";

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_NE(bound.error->message.find("JSON object"), std::string::npos);
}

TEST(FromBodyTest, RejectsNonJsonContentType)
{
    baldr::HttpRequest request;
    request.headers["content-type"] = "application/x-www-form-urlencoded";
    request.body                    = R"({"name":"Alice","age":30})";

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_FALSE(bound.isOk());
    ASSERT_TRUE(bound.error.has_value());
    EXPECT_EQ(static_cast<int>(bound.error->statusCode),
              static_cast<int>(baldr::StatusCode::UnsupportedMediaType));
}

TEST(FromBodyTest, ContentTypeMatchingIsCaseInsensitive)
{
    baldr::HttpRequest request;
    request.headers["content-type"] = "Application/JSON; charset=utf-8";
    request.body                    = R"({"name":"Alice","age":30})";

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_TRUE(bound.isOk());
    EXPECT_EQ(bound.value.name, "Alice");
}

TEST(FromBodyTest, MissingContentTypeIsPermissive)
{
    baldr::HttpRequest request;
    request.body = R"({"name":"Alice","age":30})";

    auto bound = baldr::detail::bindFromBody<UserDto>(request);

    EXPECT_TRUE(bound.isOk());
    EXPECT_EQ(bound.value.age, 30);
}

TEST(FromBodyTest, IsFromBodyTraitDetectsWrapper)
{
    static_assert(baldr::isFromBody_v<baldr::FromBody<UserDto>>,
                  "isFromBody_v must recognise FromBody<U>");
    static_assert(!baldr::isFromBody_v<UserDto>,
                  "isFromBody_v must reject non-wrapper types");
    static_assert(!baldr::isFromBody_v<int>,
                  "isFromBody_v must reject primitives");
    static_assert(
        std::is_same_v<baldr::isFromBody<baldr::FromBody<UserDto>>::ValueType,
                       UserDto>,
        "ValueType alias must surface the wrapped payload type");
}