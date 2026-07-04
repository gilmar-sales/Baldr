#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/StatusCode.hpp>

class ResultSpec : public ::testing::Test
{
};

TEST_F(ResultSpec, TextResultAppliesBodyAndContentType)
{
    baldr::TextResult   r("hello");
    baldr::HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(response.body, "hello");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::OK));
}

TEST_F(ResultSpec, JsonResultAppliesBodyAndContentType)
{
    baldr::JsonResult   r(R"({"x":1})");
    baldr::HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(response.body, R"({"x":1})");
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");
}

TEST_F(ResultSpec, StatusResultAppliesStatus)
{
    baldr::StatusResult r(baldr::StatusCode::NoContent);
    baldr::HttpResponse response;
    r.Apply(response);
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::NoContent));
    EXPECT_TRUE(response.body.empty());
}

TEST_F(ResultSpec, ResultsFactoryFunctions)
{
    auto ok   = baldr::Results::Ok("body");
    auto json = baldr::Results::Json(std::string(R"({})"));
    auto notF = baldr::Results::NotFound();
    auto stat = baldr::Results::Status(baldr::StatusCode::Accepted);

    baldr::HttpResponse response;
    ok.Apply(response);
    EXPECT_EQ(response.body, "body");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");

    response = baldr::HttpResponse();
    json.Apply(response);
    EXPECT_EQ(response.body, "\"{}\"");
    EXPECT_EQ(response.headers.at("Content-Type"), "application/json");

    response = baldr::HttpResponse();
    notF.Apply(response);
    EXPECT_EQ(response.body, "Not Found");
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::NotFound));

    response = baldr::HttpResponse();
    stat.Apply(response);
    EXPECT_TRUE(response.body.empty());
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(baldr::StatusCode::Accepted));
}

TEST_F(ResultSpec, ContentResultAppliesCustomContentType)
{
    baldr::ContentResult r("body", "image/png");
    baldr::HttpResponse  response;
    r.Apply(response);
    EXPECT_EQ(response.body, "body");
    EXPECT_EQ(response.headers.at("Content-Type"), "image/png");
}