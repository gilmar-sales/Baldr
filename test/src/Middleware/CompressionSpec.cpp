#include <Baldr/Middleware/Compression/Internal.hpp>
#include <Baldr/Middleware/Compression/Middleware.hpp>

#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>

#include <string>
#include <string_view>
#include <utility>

class CompressionSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method  = baldr::HttpMethod::Get;
        mRequest.path    = "/";
        mRequest.version = "HTTP/1.1";
        mResponse        = baldr::HttpResponse(mRequest);
        mBigBody         = std::string(4096, 'a');
    }

    void next()
    {
        // Populate a text-like body and content-type, mimicking a
        // typical handler return.
        mResponse.body                    = mBigBody;
        mResponse.headers["Content-Type"] = "text/plain; charset=utf-8";
    }

    baldr::HttpRequest  mRequest;
    baldr::HttpResponse mResponse;
    std::string         mBigBody;
};

TEST_F(CompressionSpec, GzipRoundTrip)
{
    std::string plain(8192, 'x');
    std::string encoded;
    ASSERT_TRUE(baldr::Detail::gzipCompress(plain, encoded, 6));
    EXPECT_LT(encoded.size(), plain.size());

    std::string decoded;
    ASSERT_TRUE(baldr::Detail::gzipDecompress(encoded, decoded));
    EXPECT_EQ(decoded, plain);
}

TEST_F(CompressionSpec, CompressesTextBodiesWhenAccepted)
{
    mRequest.headers["accept-encoding"] = "gzip";
    baldr::CompressionMiddleware mw;
    mw.Handle(mRequest, mResponse, [this]() { next(); });

    ASSERT_TRUE(mResponse.headers.count("Content-Encoding"));
    EXPECT_EQ(mResponse.headers.at("Content-Encoding"), "gzip");
    EXPECT_LT(mResponse.body.size(), mBigBody.size());
    EXPECT_TRUE(mResponse.headers.count("Vary"));
}

TEST_F(CompressionSpec, SkipsWhenClientDoesNotAcceptGzip)
{
    mRequest.headers["accept-encoding"] = "identity";
    baldr::CompressionMiddleware mw;
    mw.Handle(mRequest, mResponse, [this]() { next(); });

    EXPECT_EQ(mResponse.headers.count("Content-Encoding"), 0u);
}

TEST_F(CompressionSpec, SkipsZeroQValueGzip)
{
    mRequest.headers["accept-encoding"] = "gzip;q=0";
    baldr::CompressionMiddleware mw;
    mw.Handle(mRequest, mResponse, [this]() { next(); });

    EXPECT_EQ(mResponse.headers.count("Content-Encoding"), 0u);
}

TEST_F(CompressionSpec, SkipsNonTextContentType)
{
    mRequest.headers["accept-encoding"] = "gzip";
    baldr::CompressionMiddleware mw;
    mw.Handle(mRequest, mResponse, [this]() {
        mResponse.body                    = mBigBody;
        mResponse.headers["Content-Type"] = "image/png";
    });
    EXPECT_EQ(mResponse.headers.count("Content-Encoding"), 0u);
}

TEST_F(CompressionSpec, RespectsMinBodySize)
{
    mRequest.headers["accept-encoding"] = "gzip";
    baldr::CompressionOptions opts;
    opts.minBodyBytes = 10000;
    baldr::CompressionMiddleware mw(opts);
    mw.Handle(mRequest, mResponse, [this]() { next(); });
    EXPECT_EQ(mResponse.headers.count("Content-Encoding"), 0u);
}

TEST_F(CompressionSpec, SkipsNoContentStatus)
{
    mRequest.headers["accept-encoding"] = "gzip";
    baldr::CompressionMiddleware mw;
    mw.Handle(mRequest, mResponse, [this]() {
        mResponse.body                    = mBigBody;
        mResponse.headers["Content-Type"] = "text/plain";
        mResponse.statusCode              = baldr::StatusCode::NoContent;
    });
    EXPECT_EQ(mResponse.headers.count("Content-Encoding"), 0u);
}

TEST_F(CompressionSpec, SupportsWildcardAcceptEncoding)
{
    mRequest.headers["accept-encoding"] = "*";
    baldr::CompressionMiddleware mw;
    mw.Handle(mRequest, mResponse, [this]() { next(); });
    EXPECT_EQ(mResponse.headers.at("Content-Encoding"), "gzip");
}

TEST_F(CompressionSpec, MimeTypeExactMatchForJson)
{
    baldr::CompressionMiddleware mw;
    EXPECT_TRUE(baldr::CompressionMiddleware::mimeAllowedForTest(
        "application/json",
        { "application/json" }));
    EXPECT_FALSE(baldr::CompressionMiddleware::mimeAllowedForTest(
        "application/jsonp",
        { "application/json" }));
}
