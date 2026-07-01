#include "Baldr/HttpRequest.hpp"
#include "Baldr/HttpResponse.hpp"
#include "Baldr/StatusCode.hpp"

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