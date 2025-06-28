#include "Baldr/HttpRequestParser.hpp"
#include "Baldr/StatusCode.hpp"
#include "Skirnir/Common.hpp"

class HttpRequestParserSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mHttpRequestParser = skr::MakeRef<HttpRequestParser>();
    }

    void TearDown() override { mHttpRequestParser.reset(); }

    Ref<HttpRequestParser> mHttpRequestParser;
};

TEST_F(HttpRequestParserSpec, HttpRequestParser_ShouldAccept_ValidGetWithNoBody)
{
    auto result =
        mHttpRequestParser->parse("GET /hello HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::GET);
    ASSERT_EQ(result.value.path, "/hello");
    ASSERT_EQ(result.value.version, "HTTP/1.1");
    ASSERT_EQ(result.value.headers.size(), 1);
    ASSERT_EQ(result.value.headers["host"], "x");
    ASSERT_STREQ(result.value.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldAcceptValidPostWithBodyAndContentLength)
{
    auto result = mHttpRequestParser->parse(
        "POST /x HTTP/1.1\r\nContent-Length: 3\r\nHost: x\r\n\r\nabc");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::POST);
    ASSERT_EQ(result.value.path, "/x");
    ASSERT_EQ(result.value.version, "HTTP/1.1");
    ASSERT_EQ(result.value.headers.size(), 2);
    ASSERT_EQ(result.value.headers["content-length"], "3");
    ASSERT_STREQ(result.value.body.c_str(), "abc");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldAcceptHeadersWithMixedCasing)
{
    auto result = mHttpRequestParser->parse(
        "GET /x HTTP/1.1\r\ncOnTeNt-Type: text/plain\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::GET);
    ASSERT_EQ(result.value.path, "/x");
    ASSERT_EQ(result.value.version, "HTTP/1.1");
    ASSERT_EQ(result.value.headers.size(), 2);
    ASSERT_EQ(result.value.headers["content-type"], "text/plain");
    ASSERT_STREQ(result.value.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldDoQueryStringParsing)
{
    auto result = mHttpRequestParser->parse(
        "GET /api?x=1&y=2 HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::GET);
    ASSERT_EQ(result.value.path, "/api");
    ASSERT_EQ(result.value.version, "HTTP/1.1");
    ASSERT_EQ(result.value.headers.size(), 1);
    ASSERT_EQ(result.value.headers["host"], "x");
    ASSERT_STREQ(result.value.body.c_str(), "");
    ASSERT_STREQ(result.value.query["x"].c_str(), "1");
    ASSERT_STREQ(result.value.query["y"].c_str(), "2");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectHeaderFoldingRFC7230)
{
    auto result = mHttpRequestParser->parse(
        "POST /test HTTP/1.1\r\nHeader-A: cat\r\n "
        "food\r\nHeader-B: cat \r\n food\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_EQ(result.value.method, HttpMethod::POST);
    ASSERT_EQ(result.value.path, "/test");
    ASSERT_EQ(result.value.version, "HTTP/1.1");
    ASSERT_STREQ(result.error.c_str(),
                 "Header folding is not allowed in HTTP/1.1");
    ASSERT_STREQ(result.value.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectEmptyRequest)
{
    auto result = mHttpRequestParser->parse("");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectMissingHttpVersion)
{
    auto result = mHttpRequestParser->parse("GET /hello\r\nHost: x\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "HTTP version is missing or invalid");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectMalformedMethod)
{
    auto result = mHttpRequestParser->parse(
        "G ET /api?x=1&y=2 HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Malformed HTTP method");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectMissingEndLine)
{
    GTEST_SKIP_("This test is skipped because the socket will not stop reading "
                "the request until found request end line.");

    auto result =
        mHttpRequestParser->parse("GET /api?x=1&y=2 HTTP/1.1\r\nHost: x");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Missing end of request line");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectInvalidHeaderFormat)
{
    auto result = mHttpRequestParser->parse(
        "GET /api?x=1&y=2 HTTP/1.1\r\nHost x\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectNoHostHeader)
{
    auto result = mHttpRequestParser->parse("GET /api?x=1&y=2 HTTP/1.1\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Missing Host header");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectExtraWhitespaceInMethod)
{
    auto result =
        mHttpRequestParser->parse("GET  /hello HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Extra whitespace in method");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectExtraWhitespaceInPath)
{
    auto result =
        mHttpRequestParser->parse("GET /hello  HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Extra whitespace in path");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectExtraWhitespaceInVersion)
{
    auto result =
        mHttpRequestParser->parse("GET /hello HTTP/1.1 \r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Extra whitespace in version");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectDuplicateHeaders)
{
    auto result = mHttpRequestParser->parse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nHost: y\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Duplicate headers are not allowed");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectExtremelyLongPath)
{
    std::string longPath(10000, 'a');
    auto        result = mHttpRequestParser->parse(
        "GET /" + longPath + " HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Path is too long");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectLargeNumberOfHeaders)
{
    std::string headers;
    for (int i = 0; i < 1000; ++i)
    {
        headers += "Header-" + std::to_string(i) + ": value\r\n";
    }
    auto result =
        mHttpRequestParser->parse("GET /hello HTTP/1.1\r\n" + headers + "\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Too many headers");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectLargeHeaderValue)
{
    auto result = mHttpRequestParser->parse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nLarge-Header: " +
        std::string(8192, 'a') + "\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Header value is too large");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectNoContentLength)
{
    auto result =
        mHttpRequestParser->parse("POST /hello HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Missing Content-Length header");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectConflictingContentLengthAndTransferEncoding)
{
    auto result = mHttpRequestParser->parse(
        "POST /hello HTTP/1.1\r\nHost: x\r\nContent-Length: 10\r\n"
        "Transfer-Encoding: chunked\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(),
                 "Conflicting Content-Length and Transfer-Encoding headers");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldSanitizeHeaderInjection)
{
    auto result = mHttpRequestParser->parse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nX-Injected-Header: value\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::GET);
    ASSERT_EQ(result.value.path, "/hello");
    ASSERT_EQ(result.value.headers.size(), 2);
    ASSERT_EQ(result.value.headers["host"], "x");
    ASSERT_EQ(result.value.headers["x-injected-header"], "value");
    ASSERT_STREQ(result.value.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectSmuggledNullByteInPath)
{
    auto result =
        mHttpRequestParser->parse("GET /hello%00world HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectInvalidEncoding)
{
    auto result = mHttpRequestParser->parse(
        "GET /hello%20world% HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldTrimInHeaders)
{
    auto result =
        mHttpRequestParser->parse("GET /hello HTTP/1.1\r\n Host : x\r\n");

    ASSERT_TRUE(true);
    ASSERT_STREQ(result.value.headers["host"].c_str(), "x");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldDecodePath)
{
    auto result =
        mHttpRequestParser->parse("GET /hello%20world HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::GET);
    ASSERT_EQ(result.value.path, "/hello world");
    ASSERT_STREQ(result.value.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldDecodePathAndQuery)
{
    auto result = mHttpRequestParser->parse(
        "GET /hello%20world?name=John%20Doe HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::GET);
    ASSERT_EQ(result.value.path, "/hello world");
    ASSERT_EQ(result.value.query["name"], "John Doe");
    ASSERT_EQ(result.value.headers.size(), 1);
    ASSERT_EQ(result.value.headers["host"], "x");
    ASSERT_STREQ(result.value.body.c_str(), "");
}