#include "Baldr/HttpRequest.hpp"
#include "Baldr/HttpRequestParser.hpp"
#include "Baldr/HttpResponse.hpp"
#include "Baldr/StatusCode.hpp"

#include <string>

class HttpRequestResponseSpec : public ::testing::Test {};

// ============================================================================
// Default-construction contracts for HttpRequest and HttpResponse.
//
// HttpConnection::handle() always starts from a default-constructed
// HttpResponse(httpRequest). The HttpResponse contract is what guarantees
// that "no route" / "handler forgot to set status" cases degrade
// predictably. These tests pin down the observable defaults so changes
// to either struct stay compatible with HttpConnection.cpp.
// ============================================================================

TEST_F(HttpRequestResponseSpec, HttpRequestDefaultsAreEmpty)
{
    HttpRequest request;

    EXPECT_EQ(request.method, HttpMethod::Get);
    EXPECT_TRUE(request.path.empty());
    EXPECT_TRUE(request.version.empty());
    EXPECT_TRUE(request.clientIp.empty());
    EXPECT_TRUE(request.headers.empty());
    EXPECT_TRUE(request.query.empty());
    EXPECT_TRUE(request.params.empty());
    EXPECT_TRUE(request.body.empty());
}

TEST_F(HttpRequestResponseSpec, HttpResponseDefaultIsZeroStatus)
{
    // HttpConnection::handle() always wraps a fresh HttpResponse(request)
    // around the incoming request, so a default-constructed HttpResponse is
    // not on the hot path. Pin the actual default to surface accidental
    // changes to the struct layout.
    HttpResponse response;

    EXPECT_EQ(static_cast<int>(response.statusCode), 0);
    EXPECT_TRUE(response.version.empty());
    EXPECT_TRUE(response.headers.empty());
    EXPECT_TRUE(response.cookies.empty());
    EXPECT_TRUE(response.body.empty());
}

TEST_F(HttpRequestResponseSpec, HttpResponseFromRequestDefaultsToNotFound)
{
    HttpRequest request;
    request.method   = HttpMethod::Post;
    request.path     = "/api/devices";
    request.version  = "HTTP/1.1";
    request.clientIp = "10.0.0.1";
    request.headers["host"] = "example.com";
    request.body = "{}";

    HttpResponse response(request);

    // Connection inherits the protocol version so the response line keeps
    // the same major version as the request.
    EXPECT_EQ(response.version, "HTTP/1.1");
    // The HttpResponse(request) constructor sets statusCode to NotFound.
    // Handlers that do not produce a response (e.g. return void, fail JSON
    // serialization) will keep this default and the connection will emit a
    // 404 line. Pin this contract — the Devices example relies on it
    // indirectly.
    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(StatusCode::NotFound));
    // Headers/body/cookies are reset to empty.
    EXPECT_TRUE(response.headers.empty());
    EXPECT_TRUE(response.cookies.empty());
    EXPECT_TRUE(response.body.empty());
}

TEST_F(HttpRequestResponseSpec, HttpResponseMutabilityAfterConstruction)
{
    HttpRequest request;
    request.version = "HTTP/1.1";

    HttpResponse response(request);
    response.statusCode = StatusCode::OK;
    response.body       = "ok";
    response.headers["Content-Type"]   = "plain/text";
    response.headers["Content-Length"] = "2";

    EXPECT_EQ(static_cast<int>(response.statusCode),
              static_cast<int>(StatusCode::OK));
    EXPECT_EQ(response.body, "ok");
    EXPECT_EQ(response.headers.at("Content-Type"), "plain/text");
    EXPECT_EQ(response.headers.at("Content-Length"), "2");
}

// ============================================================================
// Streamed-buffer integration tests.
//
// HttpConnection::onMessage feeds bytes into an accumulator and calls
// HttpRequestParser::tryParse on the growing buffer. These tests pin down
// that behaviour: partial reads return Incomplete, the request completes
// once all bytes have arrived, and pipelined requests (a second request
// arriving on the same buffer) are also consumed.
// ============================================================================

TEST_F(HttpRequestResponseSpec, ParserReturnsIncompleteOnByteByByteFeed)
{
    HttpRequestParser parser;
    const std::string request =
        "GET /hello HTTP/1.1\r\nHost: example.com\r\n\r\n";
    std::string buffer;

    for (std::size_t i = 0; i < request.size() - 1; ++i)
    {
        buffer.push_back(request[i]);
        auto status = parser.tryParse(buffer);
        ASSERT_EQ(status.kind, HttpParseStatus::Kind::Incomplete)
            << "feed index " << i;
    }

    buffer.push_back(request.back());
    auto status = parser.tryParse(buffer);
    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    EXPECT_EQ(status.request.method, HttpMethod::Get);
    EXPECT_EQ(status.request.path, "/hello");
    EXPECT_EQ(status.request.headers.at("host"), "example.com");
    EXPECT_EQ(status.consumedBytes, buffer.size());
}

TEST_F(HttpRequestResponseSpec, ParserConsumesPipelinedRequestsInOneBuffer)
{
    HttpRequestParser parser;
    const std::string buffer =
        "GET /a HTTP/1.1\r\nHost: example.com\r\n\r\n"
        "GET /b HTTP/1.1\r\nHost: example.com\r\n\r\n";

    auto first = parser.tryParse(buffer);
    ASSERT_EQ(first.kind, HttpParseStatus::Kind::Complete);
    EXPECT_EQ(first.request.path, "/a");
    EXPECT_EQ(first.consumedBytes,
              std::string("GET /a HTTP/1.1\r\nHost: example.com\r\n\r\n").size());

    auto second = parser.tryParse(
        buffer.substr(first.consumedBytes));
    ASSERT_EQ(second.kind, HttpParseStatus::Kind::Complete);
    EXPECT_EQ(second.request.path, "/b");
}

TEST_F(HttpRequestResponseSpec, ParserChunksHeadersAtArbitraryBoundaries)
{
    HttpRequestParser parser;
    const std::string request =
        "POST /submit HTTP/1.1\r\nHost: example.com\r\n"
        "Content-Length: 5\r\n\r\nhello";

    std::string buffer;
    HttpParseStatus lastStatus {};
    for (std::size_t i = 0; i < request.size(); i += 3)
    {
        buffer.append(request, i, std::min<std::size_t>(3, request.size() - i));
        lastStatus = parser.tryParse(buffer);
    }

    ASSERT_EQ(lastStatus.kind, HttpParseStatus::Kind::Complete);
    EXPECT_EQ(lastStatus.request.method, HttpMethod::Post);
    EXPECT_EQ(lastStatus.request.path, "/submit");
    EXPECT_EQ(lastStatus.request.body, "hello");
    EXPECT_EQ(lastStatus.consumedBytes, buffer.size());
}

TEST_F(HttpRequestResponseSpec, ParserParsesSingleCookie)
{
    HttpRequestParser parser;
    auto status = parser.tryParse(
        "GET / HTTP/1.1\r\nHost: x\r\nCookie: session=abc123\r\n\r\n");
    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    EXPECT_EQ(status.request.cookies.size(), 1);
    EXPECT_EQ(status.request.cookies.at("session"), "abc123");
}

TEST_F(HttpRequestResponseSpec, ParserParsesMultipleCookies)
{
    HttpRequestParser parser;
    auto status = parser.tryParse(
        "GET / HTTP/1.1\r\nHost: x\r\n"
        "Cookie: a=1; b=2; c=3\r\n\r\n");
    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    EXPECT_EQ(status.request.cookies.size(), 3);
    EXPECT_EQ(status.request.cookies.at("a"), "1");
    EXPECT_EQ(status.request.cookies.at("b"), "2");
    EXPECT_EQ(status.request.cookies.at("c"), "3");
}

TEST_F(HttpRequestResponseSpec, ParserSkipsEmptyCookieSegments)
{
    HttpRequestParser parser;
    auto status = parser.tryParse(
        "GET / HTTP/1.1\r\nHost: x\r\n"
        "Cookie: a=1;;b=2; ;c=3\r\n\r\n");
    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    EXPECT_EQ(status.request.cookies.size(), 3);
}