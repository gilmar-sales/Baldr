#include <Baldr/Http/Results/JsonBody.hpp>

#include "UserDto.hpp"

TEST(JsonBodyTest, ParsesValidJsonObjectIntoStruct)
{
    baldr::HttpRequest request;
    request.body = R"({"name":"Alice","age":30})";

    auto result = baldr::parseJson<UserDto>(request);

    EXPECT_TRUE(result.isOk());
    EXPECT_EQ(result.value().name, "Alice");
    EXPECT_EQ(result.value().age, 30);
}

TEST(JsonBodyTest, ReportsFieldMismatch)
{
    baldr::HttpRequest request;
    request.body = R"({"name":"Bob"})"; // missing "age"

    auto result = baldr::parseJson<UserDto>(request);

    EXPECT_FALSE(result.isOk());
    EXPECT_EQ(static_cast<int>(result.error().statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
}

TEST(JsonBodyTest, RejectsEmptyBody)
{
    baldr::HttpRequest request;
    request.body = "";

    auto result = baldr::parseJsonObject(request);

    EXPECT_FALSE(result.isOk());
    EXPECT_EQ(static_cast<int>(result.error().statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_FALSE(result.error().message.empty());
}

TEST(JsonBodyTest, RejectsMalformedJson)
{
    baldr::HttpRequest request;
    request.body = "not-json";

    auto result = baldr::parseJsonObject(request);

    EXPECT_FALSE(result.isOk());
    EXPECT_EQ(static_cast<int>(result.error().statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_NE(result.error().message.find("Invalid JSON"), std::string::npos);
}

TEST(JsonBodyTest, RejectsTopLevelNonObject)
{
    baldr::HttpRequest request;
    request.body = "[1,2,3]";

    auto result = baldr::parseJsonObject(request);

    EXPECT_FALSE(result.isOk());
    EXPECT_EQ(static_cast<int>(result.error().statusCode),
              static_cast<int>(baldr::StatusCode::BadRequest));
    EXPECT_NE(result.error().message.find("JSON object"), std::string::npos);
}

TEST(JsonBodyTest, ParsesOptionalFieldPresent)
{
    baldr::HttpRequest request;
    request.body = R"({"name":"Alice","age":30,"score":42})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    ASSERT_TRUE(result.value().score.has_value());
    EXPECT_EQ(*result.value().score, 42);
}

TEST(JsonBodyTest, LeavesOptionalFieldAbsentAsNullopt)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"}})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(result.value().score.has_value());
    EXPECT_FALSE(result.value().billing.has_value());
    EXPECT_FALSE(result.value().luckyNumbers.has_value());
    EXPECT_EQ(result.value().tags.size(), 0U);
}

TEST(JsonBodyTest, TreatsExplicitJsonNullOnOptionalFieldAsNullopt)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"},"score":null})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(result.value().score.has_value());
}

TEST(JsonBodyTest, ParsesVectorOfStrings)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"},"tags":["a","b","c"]})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(result.value().tags.size(), 3U);
    EXPECT_EQ(result.value().tags[0], "a");
    EXPECT_EQ(result.value().tags[1], "b");
    EXPECT_EQ(result.value().tags[2], "c");
}

TEST(JsonBodyTest, ReportsVectorElementErrorWithArrayIndexPath)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"},"tags":["a","b",42]})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    EXPECT_FALSE(result.isOk());
    ASSERT_TRUE(result.error().field.has_value());
    EXPECT_EQ(*result.error().field, std::string("tags[2]"));
}

TEST(JsonBodyTest, AcceptsEmptyVectorAndEmptyArray)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"},"tags":[],"ratings":[7,8,9]})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().tags.size(), 0U);
    EXPECT_EQ(result.value().ratings[0], 7);
    EXPECT_EQ(result.value().ratings[1], 8);
    EXPECT_EQ(result.value().ratings[2], 9);
}

TEST(JsonBodyTest, ReportsArrayLengthMismatchWithIndexPath)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"},"ratings":[1,2]})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    EXPECT_FALSE(result.isOk());
    ASSERT_TRUE(result.error().field.has_value());
    EXPECT_EQ(*result.error().field, std::string("ratings[2]"));
}

TEST(JsonBodyTest, ReportsArrayTooLongWithIndexPath)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"},"ratings":[1,2,3,4]})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    EXPECT_FALSE(result.isOk());
    ASSERT_TRUE(result.error().field.has_value());
    EXPECT_EQ(*result.error().field, std::string("ratings[3]"));
}

TEST(JsonBodyTest, ParsesNestedStructField)
{
    baldr::HttpRequest request;
    request.body =
        R"({"name":"Alice","address":{"city":"NYC","street":"5th"}})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().address.city, "NYC");
    EXPECT_EQ(result.value().address.street, "5th");
}

TEST(JsonBodyTest, ReportsNestedFieldErrorWithDottedPath)
{
    baldr::HttpRequest request;
    request.body = R"({"name":"Alice","address":{"city":42,"street":"5th"}})";

    auto result = baldr::parseJson<NestedUserDto>(request);

    EXPECT_FALSE(result.isOk());
    ASSERT_TRUE(result.error().field.has_value());
    EXPECT_EQ(*result.error().field, std::string("address.city"));
    EXPECT_NE(result.error().message.find("address.city"), std::string::npos)
        << "body message was: " << result.error().message;
}

TEST(JsonBodyTest, ParsesOptionalNestedStructPresent)
{
    baldr::HttpRequest request;
    request.body = R"({
        "name":"Alice",
        "address":{"city":"NYC","street":"5th"},
        "billing":{"city":"LA","street":"Sunset"}
    })";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    ASSERT_TRUE(result.value().billing.has_value());
    EXPECT_EQ(result.value().billing->city, "LA");
    EXPECT_EQ(result.value().billing->street, "Sunset");
}

TEST(JsonBodyTest, ParsesOptionalVectorPresent)
{
    baldr::HttpRequest request;
    request.body = R"({
        "name":"Alice",
        "address":{"city":"NYC","street":"5th"},
        "luckyNumbers":[1,2,3]
    })";

    auto result = baldr::parseJson<NestedUserDto>(request);

    ASSERT_TRUE(result.isOk());
    ASSERT_TRUE(result.value().luckyNumbers.has_value());
    EXPECT_EQ(result.value().luckyNumbers->size(), 3U);
    EXPECT_EQ((*result.value().luckyNumbers)[0], 1);
}

TEST(JsonBodyTest, ReportsErrorInsideOptionalNestedStructWithFullPath)
{
    baldr::HttpRequest request;
    request.body = R"({
        "name":"Alice",
        "address":{"city":"NYC","street":"5th"},
        "billing":{"city":99,"street":"Sunset"}
    })";

    auto result = baldr::parseJson<NestedUserDto>(request);

    EXPECT_FALSE(result.isOk());
    ASSERT_TRUE(result.error().field.has_value());
    EXPECT_EQ(*result.error().field, std::string("billing.city"));
}

TEST(JsonBodyTest, OptionalDtoRequiresAllOptionalsMayBeAbsent)
{
    baldr::HttpRequest request;
    request.body = R"({})";

    auto result = baldr::parseJson<OptionalOnlyDto>(request);

    ASSERT_TRUE(result.isOk());
    EXPECT_FALSE(result.value().maybeInt.has_value());
    EXPECT_FALSE(result.value().maybeString.has_value());
    EXPECT_FALSE(result.value().maybeAddress.has_value());
}

TEST(JsonBodyTest, OptionalDtoMixesPresentAndAbsentFields)
{
    baldr::HttpRequest request;
    request.body =
        R"({"maybeInt":7,"maybeAddress":{"city":"NYC","street":"5th"}})";

    auto result = baldr::parseJson<OptionalOnlyDto>(request);

    ASSERT_TRUE(result.isOk());
    ASSERT_TRUE(result.value().maybeInt.has_value());
    EXPECT_EQ(*result.value().maybeInt, 7);
    EXPECT_FALSE(result.value().maybeString.has_value());
    ASSERT_TRUE(result.value().maybeAddress.has_value());
    EXPECT_EQ(result.value().maybeAddress->city, "NYC");
}
