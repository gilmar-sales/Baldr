#include <Baldr/Http/Results/JsonBody.hpp>

#include <string>

struct UserDto
{
    std::string name;
    int         age = 0;
};

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
