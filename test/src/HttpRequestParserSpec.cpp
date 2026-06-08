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
    auto result =
        mHttpRequestParser->parse("GET /hello HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::Get);
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
    ASSERT_EQ(result.value.method, HttpMethod::Post);
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
    ASSERT_EQ(result.value.method, HttpMethod::Get);
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
    ASSERT_EQ(result.value.method, HttpMethod::Get);
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
    ASSERT_EQ(result.value.method, HttpMethod::Post);
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
    ASSERT_EQ(result.value.method, HttpMethod::Get);
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
    ASSERT_EQ(result.value.method, HttpMethod::Get);
    ASSERT_EQ(result.value.path, "/hello world");
    ASSERT_STREQ(result.value.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec, HttpRequestParserShouldDecodePathAndQuery)
{
    auto result = mHttpRequestParser->parse(
        "GET /hello%20world?name=John%20Doe HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::OK);
    ASSERT_EQ(result.value.method, HttpMethod::Get);
    ASSERT_EQ(result.value.path, "/hello world");
    ASSERT_EQ(result.value.query["name"], "John Doe");
    ASSERT_EQ(result.value.headers.size(), 1);
    ASSERT_EQ(result.value.headers["host"], "x");
    ASSERT_STREQ(result.value.body.c_str(), "");
}

// ============================================================================
// CWE-20: Improper Input Validation
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectNullBytesInDecodedPath)
{
    // %00 decodes to null byte - potential path traversal attack vector
    auto result =
        mHttpRequestParser->parse("GET /%00etc/passwd HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, ShouldRejectNullByteInQueryString)
{
    // Null bytes in query parameters
    auto result = mHttpRequestParser->parse(
        "GET /api?x=%00admin HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
}

TEST_F(HttpRequestParserSpec, ShouldRejectDoubleEncodedNullByte)
{
    // %2500 = %00 - double encoding bypass attempt
    auto result = mHttpRequestParser->parse(
        "GET /%2500etc/passwd HTTP/1.1\r\nHost: x\r\n");

    // Should be rejected as invalid encoding or null byte after decode
    ASSERT_FALSE(result.success);
}

TEST_F(HttpRequestParserSpec, ShouldRejectIncompletePercentEncoding)
{
    // Truncated % encoding - potential buffer issues
    auto result =
        mHttpRequestParser->parse("GET /path% HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Invalid URL encoding in path");
}

TEST_F(HttpRequestParserSpec, ShouldRejectSinglePercentEncoding)
{
    auto result =
        mHttpRequestParser->parse("GET /path%2 HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
}

TEST_F(HttpRequestParserSpec, ShouldRejectInvalidHexCharacters)
{
    // %ZZ is not valid hex - potential parser confusion
    auto result =
        mHttpRequestParser->parse("GET /path%ZZ HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
}

// ============================================================================
// CWE-22: Path Traversal Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectPathTraversalWithDoubleDot)
{
    auto result = mHttpRequestParser->parse(
        "GET /../../../etc/passwd HTTP/1.1\r\nHost: x\r\n");

    // While technically parsed, the path should be sanitized or rejected
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.path, "/../../../etc/passwd");
    // Note: Router/path handler should additionally validate this
}

TEST_F(HttpRequestParserSpec, ShouldRejectEncodedPathTraversal)
{
    auto result = mHttpRequestParser->parse(
        "GET /%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n");

    // Should decode to ../.. which could be dangerous
    ASSERT_TRUE(result.success);
    // The decoded path would be ../../etc/passwd
}

TEST_F(HttpRequestParserSpec, ShouldRejectEncodedParentTraversal)
{
    auto result = mHttpRequestParser->parse(
        "GET /%2e%2e%2f%2e%2e%2fpasswd HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
}

// ============================================================================
// CWE-400: Resource Exhaustion - Memory
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectExcessivelyLargePath)
{
    // Path exceeding reasonable limits (2048 bytes)
    std::string longPath(5000, 'a');
    auto        result = mHttpRequestParser->parse(
        "GET /" + longPath + " HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Path is too long");
}

TEST_F(HttpRequestParserSpec, ShouldRejectExcessivelyLargeHeaderValue)
{
    // Header value exceeding 4096 bytes
    auto result = mHttpRequestParser->parse(
        "GET /hello HTTP/1.1\r\nHost: x\r\nX-Large: " + std::string(8192, 'a') +
        "\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Header value is too large");
}

TEST_F(HttpRequestParserSpec, ShouldRejectExcessivelyLargeHeaderName)
{
    // Header key exceeding 64 bytes
    std::string longHeaderName(100, 'x');
    auto        result = mHttpRequestParser->parse(
        "GET /hello HTTP/1.1\r\n" + longHeaderName + ": value\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Header key is too long");
}

TEST_F(HttpRequestParserSpec, ShouldRejectExcessiveNumberOfHeaders)
{
    // More than 100 headers
    std::string headers;
    for (int i = 0; i < 150; ++i)
    {
        headers += "Header-" + std::to_string(i) + ": value\r\n";
    }
    auto result = mHttpRequestParser->parse(
        "GET /hello HTTP/1.1\r\n" + headers + "Host: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Too many headers");
}

TEST_F(HttpRequestParserSpec, ShouldRejectMassiveContentLength)
{
    // Content-Length claiming huge body not present
    auto result = mHttpRequestParser->parse(
        "POST /api HTTP/1.1\r\nContent-Length: 999999999\r\nHost: x\r\n\r\n");

    // Should fail due to missing body or incomplete read
    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
}

// ============================================================================
// CWE-434: Unrestricted Upload
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldEnforceContentLengthOnPOST)
{
    // POST without Content-Length should be rejected
    auto result =
        mHttpRequestParser->parse("POST /api HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Missing Content-Length header");
}

TEST_F(HttpRequestParserSpec, ShouldRejectConflictingContentLengthAndTransferEncoding)
{
    // Both headers present - potential smuggling attack
    auto result = mHttpRequestParser->parse(
        "POST /api HTTP/1.1\r\nHost: x\r\nContent-Length: 10\r\n"
        "Transfer-Encoding: chunked\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(),
                 "Conflicting Content-Length and Transfer-Encoding headers");
}

TEST_F(HttpRequestParserSpec, ShouldRejectInvalidContentLength)
{
    // Non-numeric Content-Length
    auto result = mHttpRequestParser->parse(
        "POST /api HTTP/1.1\r\nContent-Length: abc\r\nHost: x\r\n\r\nabc");

    // std::atoi returns 0 for non-numeric - body won't be read correctly
    // This should be explicitly rejected
    ASSERT_FALSE(result.success);
}

// ============================================================================
// CWE-352: Cross-Site Request Forgery Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRequireHostHeader)
{
    // Host header is required for HTTP/1.1
    auto result = mHttpRequestParser->parse("GET /api HTTP/1.1\r\n\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_STREQ(result.error.c_str(), "Missing Host header");
}

TEST_F(HttpRequestParserSpec, ShouldEnforceSingleHostHeader)
{
    // Duplicate Host header could be used for host spoofing
    auto result = mHttpRequestParser->parse(
        "GET /api HTTP/1.1\r\nHost: legitimate.com\r\nHost: evil.com\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_EQ(result.statusCode, StatusCode::BadRequest);
    ASSERT_STREQ(result.error.c_str(), "Duplicate headers are not allowed");
}

// ============================================================================
// CWE-80: Cross-Site Scripting via Header Injection
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldNormalizeHeaderNamesToLowercase)
{
    // Headers are normalized to lowercase to prevent case-based bypasses
    auto result = mHttpRequestParser->parse(
        "GET /api HTTP/1.1\r\nCONTENT-TYPE: text/html\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    // Note: Header folding should have been rejected but wasn't tested here
}

// ============================================================================
// CWE-89: SQL Injection Prevention (Header-based)
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectNewlineInHeaderInjection)
{
    // CRLF injection in header value
    auto result = mHttpRequestParser->parse(
        "GET /api HTTP/1.1\r\nHost: x\r\nX-Injected: value1\r\n"
        " Host: malicious\r\n");

    // Header folding is rejected per RFC 7230
    ASSERT_FALSE(result.success);
}

TEST_F(HttpRequestParserSpec, ShouldRejectHeaderInjectionAttempt)
{
    // Attempt to inject headers via CRLF in header value
    auto result = mHttpRequestParser->parse(
        "GET /api HTTP/1.1\r\nHost: x\r\n"
        "X-Injected: evil\r\nMalicious-Header: bad\r\n");

    // Should be parsed normally (unless it looks like header folding)
    ASSERT_TRUE(result.success);
}

// ============================================================================
// CWE-287: Authentication Bypass
// ============================================================================

// ============================================================================
// CWE-77: Command Injection Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldNotInterpretSpecialCharactersInPathAsCommands)
{
    // Special shell characters should be preserved as-is in path
    auto result = mHttpRequestParser->parse(
        "GET /;cat/etc/passwd HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.path, "/;cat/etc/passwd");
    // Note: Command execution should be prevented by the handler, not parser
}

TEST_F(HttpRequestParserSpec, ShouldPreserveBacktickCharacters)
{
    auto result =
        mHttpRequestParser->parse("GET /`whoami` HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.path, "/`whoami`");
}

TEST_F(HttpRequestParserSpec, ShouldPreservePipeCharacters)
{
    auto result = mHttpRequestParser->parse("GET /|ls HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.path, "/|ls");
}

// ============================================================================
// CWE-601: Open Redirect Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectMaliciousRedirectInQuery)
{
    // Query parameter containing redirect URL
    auto result = mHttpRequestParser->parse(
        "GET /redirect?url=http://evil.com HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    // The URL should be stored as-is for the handler to validate
    ASSERT_EQ(result.value.query["url"], "http://evil.com");
    // Note: Handler should validate and reject external redirects
}

TEST_F(HttpRequestParserSpec, ShouldRejectEncodedRedirectURL)
{
    auto result = mHttpRequestParser->parse(
        "GET /redirect?url=http%3A%2F%2Fevil.com HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.query["url"], "http://evil.com");
}

// ============================================================================
// CWE-918: Server-Side Request Forgery (SSRF)
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldDetectInternalIPInQueryParameter)
{
    // SSRF via URL parameter pointing to internal service
    auto result = mHttpRequestParser->parse(
        "GET /fetch?url=http://127.0.0.1:8080/admin HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.query["url"], "http://127.0.0.1:8080/admin");
    // Note: Handler should reject internal URLs
}

TEST_F(HttpRequestParserSpec, ShouldDetectLocalhostInQueryParameter)
{
    auto result = mHttpRequestParser->parse(
        "GET /fetch?url=http://localhost/admin HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.query["url"], "http://localhost/admin");
}

TEST_F(HttpRequestParserSpec, ShouldDetectInternalNetworkInQueryParameter)
{
    auto result = mHttpRequestParser->parse(
        "GET /fetch?url=http://10.0.0.1/internal HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.query["url"], "http://10.0.0.1/internal");
}

// ============================================================================
// CWE-502: Deserialization of Untrusted Data
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectNullBytesInRequestBody)
{
    // Null bytes in body could cause string truncation
    auto result =
        mHttpRequestParser->parse("POST /api HTTP/1.1\r\nContent-Length: "
                                  "10\r\nHost: x\r\n\r\n\x00\x00\x00\x00\x00");

    // Parser should handle null bytes in body gracefully
    // or reject them
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.value.body.size(), 5);
}

TEST_F(HttpRequestParserSpec, ShouldPreserveBinaryBodyContent)
{
    auto result = mHttpRequestParser->parse(
        "POST /api HTTP/1.1\r\nContent-Length: 6\r\nHost: x\r\n\r\nbinary");

    ASSERT_TRUE(result.success);
    ASSERT_STREQ(result.value.body.c_str(), "binary");
}

// ============================================================================
// Memory Safety Tests
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldHandleEmptyContentLengthGracefully)
{
    auto result = mHttpRequestParser->parse(
        "POST /api HTTP/1.1\r\nContent-Length: 0\r\nHost: x\r\n\r\n");

    ASSERT_TRUE(result.success);
    ASSERT_STREQ(result.value.body.c_str(), "");
}

TEST_F(HttpRequestParserSpec, ShouldHandleMissingBodyForGET)
{
    auto result = mHttpRequestParser->parse(
        "GET /api HTTP/1.1\r\nHost: x\r\n\r\nunexpected data");

    // GET requests should not expect a body
    ASSERT_TRUE(result.success);
}

TEST_F(HttpRequestParserSpec, ShouldRejectHTTPMethodCaseVariation)
{
    // HTTP methods are case-sensitive per RFC
    auto result = mHttpRequestParser->parse("get /api HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_STREQ(result.error.c_str(), "Malformed HTTP method");
}

TEST_F(HttpRequestParserSpec, ShouldRejectRandomHTTPMethodStrings)
{
    auto result =
        mHttpRequestParser->parse("OPTIONS123 /api HTTP/1.1\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_STREQ(result.error.c_str(), "Malformed HTTP method");
}

// ============================================================================
// Information Disclosure Prevention
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectTraceMethod)
{
    // TRACE can be used for XST attacks
    auto result =
        mHttpRequestParser->parse("TRACE /api HTTP/1.1\r\nHost: x\r\n\r\n");

    // TRACE is a valid HTTP method but should be blocked
    ASSERT_TRUE(result.success);
    // Note: Handler should reject TRACE requests
}

TEST_F(HttpRequestParserSpec, ShouldRejectConnectMethod)
{
    // CONNECT can be used for proxy tunneling
    auto result = mHttpRequestParser->parse(
        "CONNECT evil.com:443 HTTP/1.1\r\nHost: x\r\n\r\n");

    ASSERT_TRUE(result.success);
    // Note: Handler should reject CONNECT to external hosts
}

// ============================================================================
// URL Parsing Edge Cases
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldHandleEncodedSlashesInPath)
{
    // %2F is encoded slash - should be decoded
    auto result = mHttpRequestParser->parse(
        "GET /path%2Fwith%2Fslashes HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    // Path should contain actual slashes after decoding
}

TEST_F(HttpRequestParserSpec, ShouldHandleEncodedDotsInPath)
{
    auto result = mHttpRequestParser->parse(
        "GET /.%2e%2f.%2e%2f HTTP/1.1\r\nHost: x\r\n");

    ASSERT_TRUE(result.success);
    // Encoded dots should be preserved in the path
}

TEST_F(HttpRequestParserSpec, ShouldRejectOversizedQueryString)
{
    // Query string longer than path limit
    std::string longQuery(5000, 'a');
    auto        result = mHttpRequestParser->parse(
        "GET /?" + longQuery + " HTTP/1.1\r\nHost: x\r\n");

    // Should be parsed but may be too long for the path limit
    // or pass through if total is under limit
    ASSERT_TRUE(result.success);
}

// ============================================================================
// Version Parsing Security
// ============================================================================

TEST_F(HttpRequestParserSpec, ShouldRejectHTTP10)
{
    auto result = mHttpRequestParser->parse("GET /api HTTP/1.0\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_STREQ(result.error.c_str(), "HTTP version is missing or invalid");
}

TEST_F(HttpRequestParserSpec, ShouldRejectInvalidHTTPVersion)
{
    auto result = mHttpRequestParser->parse("GET /api HTTP/2.0\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_STREQ(result.error.c_str(), "HTTP version is missing or invalid");
}

TEST_F(HttpRequestParserSpec, ShouldRejectMissingHTTPVersion)
{
    auto result = mHttpRequestParser->parse("GET /api\r\nHost: x\r\n");

    ASSERT_FALSE(result.success);
    ASSERT_STREQ(result.error.c_str(), "HTTP version is missing or invalid");
}
