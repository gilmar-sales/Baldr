#include "Baldr/HttpRequestParser.hpp"
#include "Baldr/StatusCode.hpp"
#include "Skirnir/Common.hpp"

class HttpRequestParserSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mHttpRequestParser = skr::MakeArc<HttpRequestParser>();
    }

    void TearDown() override { mHttpRequestParser.reset(); }

    skr::Arc<HttpRequestParser> mHttpRequestParser;
};

TEST_F(HttpRequestParserSpec, HttpRequestParser_ShouldAccept_ValidGetWithNoBody)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.statusCode, StatusCode::OK);
    ASSERT_EQ(status.request.method, HttpMethod::Get);
    ASSERT_EQ(status.request.path, "/hello");
    ASSERT_EQ(status.request.version, "HTTP/1.1");
    ASSERT_EQ(status.request.headers.size(), 1);
    ASSERT_EQ(status.request.headers["host"], "x");
    ASSERT_STREQ(status.request.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldAcceptValidPostWithBodyAndContentLength)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /x HTTP/1.1\r\nContent-Length: 3\r\nHost: x\r\n\r\nabc");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.statusCode, StatusCode::OK);
    ASSERT_EQ(status.request.method, HttpMethod::Post);
    ASSERT_EQ(status.request.path, "/x");
    ASSERT_EQ(status.request.version, "HTTP/1.1");
    ASSERT_EQ(status.request.headers.size(), 2);
    ASSERT_EQ(status.request.headers["content-length"], "3");
    ASSERT_STREQ(status.request.body.c_str(), "abc");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldAcceptHeadersWithMixedCasing)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /x HTTP/1.1\r\ncOnTeNt-Type: text/plain\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.statusCode, StatusCode::OK);
    ASSERT_EQ(status.request.method, HttpMethod::Get);
    ASSERT_EQ(status.request.path, "/x");
    ASSERT_EQ(status.request.version, "HTTP/1.1");
    ASSERT_EQ(status.request.headers.size(), 2);
    ASSERT_EQ(status.request.headers["content-type"], "text/plain");
    ASSERT_STREQ(status.request.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldDoQueryStringParsing)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api?x=1&y=2 HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.statusCode, StatusCode::OK);
    ASSERT_EQ(status.request.method, HttpMethod::Get);
    ASSERT_EQ(status.request.path, "/api");
    ASSERT_EQ(status.request.version, "HTTP/1.1");
    ASSERT_EQ(status.request.headers.size(), 1);
    ASSERT_EQ(status.request.headers["host"], "x");
    ASSERT_STREQ(status.request.body.c_str(), "");
    ASSERT_STREQ(status.request.query["x"].c_str(), "1");
    ASSERT_STREQ(status.request.query["y"].c_str(), "2");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectHeaderFoldingRFC7230)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /test HTTP/1.1\r\nHeader-A: cat\r\n "
        "food\r\nHeader-B: cat \r\n food\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "Header folding is not allowed in HTTP/1.1");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectEmptyRequest)
{
    auto status = mHttpRequestParser->tryParse("");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Incomplete);
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectMissingHttpVersion)
{
    auto status =
        mHttpRequestParser->tryParse("GET /hello\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "HTTP version is missing or invalid");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectMalformedMethod)
{
    auto status = mHttpRequestParser->tryParse(
        "G ET /api?x=1&y=2 HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Malformed HTTP method");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectMissingEndLine)
{
    auto status =
        mHttpRequestParser->tryParse("GET /api?x=1&y=2 HTTP/1.1\r\nHost: x");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Incomplete);
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectInvalidHeaderFormat)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api?x=1&y=2 HTTP/1.1\r\nHost x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectNoHostHeader)
{
    auto status =
        mHttpRequestParser->tryParse(
            "GET /api?x=1&y=2 HTTP/1.1\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Missing Host header");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectExtraWhitespaceInMethod)
{
    auto status = mHttpRequestParser->tryParse(
        "GET  /hello HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Extra whitespace in method");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectExtraWhitespaceInPath)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello  HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Extra whitespace in path");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectExtraWhitespaceInVersion)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1 \r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Extra whitespace in version");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectDuplicateHeaders)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nHost: y\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "Duplicate headers are not allowed");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectExtremelyLongPath)
{
    std::string longPath(10000, 'a');
    auto        status = mHttpRequestParser->tryParse(
        "GET /" + longPath + " HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Path is too long");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectLargeNumberOfHeaders)
{
    std::string headers;
    for (int i = 0; i < 1000; ++i)
    {
        headers += "Header-" + std::to_string(i) + ": value\r\n";
    }
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\n" + headers + "\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Too many headers");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectLargeHeaderValue)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nLarge-Header: " +
        std::string(8192, 'a') + "\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Header value is too large");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectNoContentLength)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /hello HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Missing Content-Length header");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectConflictingContentLengthAndTransferEncoding)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /hello HTTP/1.1\r\nHost: x\r\nContent-Length: 10\r\n"
        "Transfer-Encoding: chunked\r\n\r\n0123456789");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "Conflicting Content-Length and Transfer-Encoding headers");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldSanitizeHeaderInjection)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nX-Injected-Header: value\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.statusCode, StatusCode::OK);
    ASSERT_EQ(status.request.method, HttpMethod::Get);
    ASSERT_EQ(status.request.path, "/hello");
    ASSERT_EQ(status.request.headers.size(), 2);
    ASSERT_EQ(status.request.headers["host"], "x");
    ASSERT_EQ(status.request.headers["x-injected-header"], "value");
    ASSERT_STREQ(status.request.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec,
       HttpRequestParserShouldRejectSmuggledNullByteInPath)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello%00world HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldRejectInvalidEncoding)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello%20world% HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldTrimInHeaders)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\n Host : x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_STREQ(status.request.headers["host"].c_str(), "x");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldDecodePath)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello%20world HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.statusCode, StatusCode::OK);
    ASSERT_EQ(status.request.method, HttpMethod::Get);
    ASSERT_EQ(status.request.path, "/hello world");
    ASSERT_STREQ(status.request.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldDecodePathAndQuery)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello%20world?name=John%20Doe HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.statusCode, StatusCode::OK);
    ASSERT_EQ(status.request.method, HttpMethod::Get);
    ASSERT_EQ(status.request.path, "/hello world");
    ASSERT_EQ(status.request.query["name"], "John Doe");
    ASSERT_EQ(status.request.headers.size(), 1);
    ASSERT_EQ(status.request.headers["host"], "x");
    ASSERT_STREQ(status.request.body.c_str(), "");
}

// ============================================================================
// CWE-20: Improper Input Validation
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectNullBytesInDecodedPath)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /%00etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, ShouldRejectNullByteInQueryString)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api?x=%00admin HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
}

TEST_F(HttpRequestParserSpec, ShouldRejectDoubleEncodedNullByte)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /%2500etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, ShouldRejectIncompletePercentEncoding)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /path% HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, ShouldRejectSinglePercentEncoding)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /path%2 HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
}

TEST_F(HttpRequestParserSpec, ShouldRejectInvalidHexCharacters)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /path%ZZ HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
}

// ============================================================================
// CWE-22: Path Traversal Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectPathTraversalWithDoubleDot)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /../../../etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.path, "/../../../etc/passwd");
}

TEST_F(HttpRequestParserSpec, ShouldRejectEncodedPathTraversal)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

TEST_F(HttpRequestParserSpec, ShouldRejectEncodedParentTraversal)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /%2e%2e%2f%2e%2e%2fpasswd HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

// ============================================================================
// CWE-400: Resource Exhaustion - Memory
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectExcessivelyLargePath)
{
    std::string longPath(5000, 'a');
    auto        status = mHttpRequestParser->tryParse(
        "GET /" + longPath + " HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Path is too long");
}

TEST_F(HttpRequestParserSpec, ShouldRejectExcessivelyLargeHeaderValue)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nX-Large: " + std::string(8192, 'a') +
        "\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Header value is too large");
}

TEST_F(HttpRequestParserSpec, ShouldRejectExcessivelyLargeHeaderName)
{
    std::string longHeaderName(100, 'x');
    auto        status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\n" + longHeaderName + ": value\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Header key is too long");
}

TEST_F(HttpRequestParserSpec, ShouldRejectExcessiveNumberOfHeaders)
{
    std::string headers;
    for (int i = 0; i < 150; ++i)
    {
        headers += "Header-" + std::to_string(i) + ": value\r\n";
    }
    auto status = mHttpRequestParser->tryParse(
        "GET /hello HTTP/1.1\r\n" + headers + "Host: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Too many headers");
}

TEST_F(HttpRequestParserSpec, ShouldRejectMassiveContentLength)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /api HTTP/1.1\r\nContent-Length: 999999999\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
}

// ============================================================================
// CWE-434: Unrestricted Upload
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldEnforceContentLengthOnPOST)
{
    auto status =
        mHttpRequestParser->tryParse("POST /api HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Missing Content-Length header");
}

TEST_F(HttpRequestParserSpec,
       ShouldRejectConflictingContentLengthAndTransferEncoding)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /api HTTP/1.1\r\nHost: x\r\nContent-Length: 10\r\n"
        "Transfer-Encoding: chunked\r\n\r\n0123456789");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "Conflicting Content-Length and Transfer-Encoding headers");
}

TEST_F(HttpRequestParserSpec, ShouldRejectInvalidContentLength)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /api HTTP/1.1\r\nContent-Length: abc\r\nHost: x\r\n\r\nabc");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
}

// ============================================================================
// CWE-352: Cross-Site Request Forgery Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRequireHostHeader)
{
    auto status = mHttpRequestParser->tryParse("GET /api HTTP/1.1\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_STREQ(status.errorMessage.c_str(), "Missing Host header");
}

TEST_F(HttpRequestParserSpec, ShouldEnforceSingleHostHeader)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api HTTP/1.1\r\nHost: legitimate.com\r\nHost: evil.com\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "Duplicate headers are not allowed");
}

// ============================================================================
// CWE-80: Cross-Site Scripting via Header Injection
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldNormalizeHeaderNamesToLowercase)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api HTTP/1.1\r\nCONTENT-TYPE: text/html\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

// ============================================================================
// CWE-89: SQL Injection Prevention (Header-based)
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectNewlineInHeaderInjection)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api HTTP/1.1\r\nHost: x\r\nX-Injected: value1\r\n"
        " Host: malicious\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
}

TEST_F(HttpRequestParserSpec, ShouldRejectHeaderInjectionAttempt)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api HTTP/1.1\r\nHost: x\r\n"
        "X-Injected: evil\r\nMalicious-Header: bad\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

// ============================================================================
// CWE-287: Authentication Bypass
// ============================================================================

// ============================================================================
// CWE-77: Command Injection Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec,
       ShouldNotInterpretSpecialCharactersInPathAsCommands)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /;cat/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.path, "/;cat/etc/passwd");
}

TEST_F(HttpRequestParserSpec, ShouldPreserveBacktickCharacters)
{
    auto status =
        mHttpRequestParser->tryParse("GET /`whoami` HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.path, "/`whoami`");
}

TEST_F(HttpRequestParserSpec, ShouldPreservePipeCharacters)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /|ls HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.path, "/|ls");
}

// ============================================================================
// CWE-601: Open Redirect Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectMaliciousRedirectInQuery)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /redirect?url=http://evil.com HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.query["url"], "http://evil.com");
}

TEST_F(HttpRequestParserSpec, ShouldRejectEncodedRedirectURL)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /redirect?url=http%3A%2F%2Fevil.com HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.query["url"], "http://evil.com");
}

// ============================================================================
// CWE-918: Server-Side Request Forgery (SSRF)
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldDetectInternalIPInQueryParameter)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /fetch?url=http://127.0.0.1:8080/admin HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.query["url"], "http://127.0.0.1:8080/admin");
}

TEST_F(HttpRequestParserSpec, ShouldDetectLocalhostInQueryParameter)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /fetch?url=http://localhost/admin HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.query["url"], "http://localhost/admin");
}

TEST_F(HttpRequestParserSpec, ShouldDetectInternalNetworkInQueryParameter)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /fetch?url=http://10.0.0.1/internal HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_EQ(status.request.query["url"], "http://10.0.0.1/internal");
}

// ============================================================================
// CWE-502: Deserialization of Untrusted Data
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectNullBytesInRequestBody)
{
    // Content-Length claims 10 bytes but only 5 follow; tryParse reports
    // Incomplete (need more bytes) rather than Error.
    auto status = mHttpRequestParser->tryParse(
        "POST /api HTTP/1.1\r\nContent-Length: "
        "10\r\nHost: x\r\n\r\n\x00\x00\x00\x00\x00");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Incomplete);
}

TEST_F(HttpRequestParserSpec, ShouldPreserveBinaryBodyContent)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /api HTTP/1.1\r\nContent-Length: 6\r\nHost: x\r\n\r\nbinary");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
    ASSERT_STREQ(status.request.body.c_str(), "binary");
}

// ============================================================================
// Memory Safety Tests
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectZeroContentLength)
{
    auto status = mHttpRequestParser->tryParse(
        "POST /api HTTP/1.1\r\nContent-Length: 0\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_EQ(status.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(status.errorMessage.c_str(), "Invalid Content-Length header");
}

TEST_F(HttpRequestParserSpec, ShouldHandleMissingBodyForGET)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /api HTTP/1.1\r\nHost: x\r\n\r\nunexpected data");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

TEST_F(HttpRequestParserSpec, ShouldRejectHTTPMethodCaseVariation)
{
    auto status =
        mHttpRequestParser->tryParse("get /api HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_STREQ(status.errorMessage.c_str(), "Malformed HTTP method");
}

TEST_F(HttpRequestParserSpec, ShouldRejectRandomHTTPMethodStrings)
{
    auto status = mHttpRequestParser->tryParse(
        "OPTIONS123 /api HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_STREQ(status.errorMessage.c_str(), "Malformed HTTP method");
}

// ============================================================================
// Information Disclosure Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectTraceMethod)
{
    auto status =
        mHttpRequestParser->tryParse("TRACE /api HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

TEST_F(HttpRequestParserSpec, ShouldRejectConnectMethod)
{
    auto status = mHttpRequestParser->tryParse(
        "CONNECT evil.com:443 HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

// ============================================================================
// URL Parsing Edge Cases
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldHandleEncodedSlashesInPath)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /path%2Fwith%2Fslashes HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

TEST_F(HttpRequestParserSpec, ShouldHandleEncodedDotsInPath)
{
    auto status = mHttpRequestParser->tryParse(
        "GET /.%2e%2f.%2e%2f HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Complete);
}

TEST_F(HttpRequestParserSpec, ShouldRejectOversizedQueryString)
{
    std::string longQuery(5000, 'a');
    auto        status = mHttpRequestParser->tryParse(
        "GET /?" + longQuery + " HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
}

// ============================================================================
// Version Parsing Security
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectHTTP10)
{
    auto status =
        mHttpRequestParser->tryParse("GET /api HTTP/1.0\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "HTTP version is missing or invalid");
}

TEST_F(HttpRequestParserSpec, ShouldRejectInvalidHTTPVersion)
{
    auto status =
        mHttpRequestParser->tryParse("GET /api HTTP/2.0\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "HTTP version is missing or invalid");
}

TEST_F(HttpRequestParserSpec, ShouldRejectMissingHTTPVersion)
{
    auto status = mHttpRequestParser->tryParse("GET /api\r\nHost: x\r\n\r\n");

    ASSERT_EQ(status.kind, HttpParseStatus::Kind::Error);
    ASSERT_STREQ(status.errorMessage.c_str(),
                 "HTTP version is missing or invalid");
}
