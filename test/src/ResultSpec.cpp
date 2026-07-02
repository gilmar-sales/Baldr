#include "Baldr/HttpResponse.hpp"
#include "Baldr/Result.hpp"
#include "Baldr/StatusCode.hpp"

class ResultSpec : public ::testing::Test {};

TEST_F(ResultSpec, TextResultAppliesBodyAndContentType)
{
    TextResult r("hello");
    HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(response.body, "hello");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(StatusCode::OK));
}

TEST_F(ResultSpec, JsonResultAppliesBodyAndContentType)
{
    JsonResult r(R"({"x":1})");
    HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(response.body, R"({"x":1})");
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST_F(ResultSpec, StatusResultAppliesStatus)
{
    StatusResult r(StatusCode::NoContent);
    HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(StatusCode::NoContent));
    EXPECT_TRUE(response.body.empty());
}

TEST_F(ResultSpec, ResultsFactoryFunctions)
{
    auto ok    = Results::Ok("body");
    auto json  = Results::Json(R"({})");
    auto notF  = Results::NotFound();
    auto stat  = Results::Status(StatusCode::Accepted);

    HttpResponse response;
    ok.Apply(response);
    EXPECT_EQ(response.body, "body");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");

    response = HttpResponse();
    json.Apply(response);
    EXPECT_EQ(response.body, "{}");
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");

    response = HttpResponse();
    notF.Apply(response);
    EXPECT_EQ(response.body, "Not Found");
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(StatusCode::NotFound));

    response = HttpResponse();
    stat.Apply(response);
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(StatusCode::Accepted));
}

TEST_F(ResultSpec, ContentResultAppliesCustomContentType)
{
    ContentResult r("body", "image/png");
    HttpResponse   response;
    r.Apply(response);
    EXPECT_EQ(response.body, "body");
    EXPECT_EQ(response.headers.at("Content-Type"), "image/png");
}