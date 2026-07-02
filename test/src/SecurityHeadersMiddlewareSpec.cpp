#include "Baldr/SecurityHeadersMiddleware.hpp"

#include "Baldr/HttpMethod.hpp"
#include "Baldr/HttpRequest.hpp"
#include "Baldr/HttpResponse.hpp"

class SecurityHeadersMiddlewareSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method  = HttpMethod::Get;
        mRequest.path    = "/";
        mRequest.version = "HTTP/1.1";
        mResponse        = HttpResponse(mRequest);
    }

    HttpRequest  mRequest;
    HttpResponse mResponse;
};

TEST_F(SecurityHeadersMiddlewareSpec, AppliesDefaultHeaders)
{
    SecurityHeadersMiddleware mw;
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.at("X-Content-Type-Options"), "nosniff");
    EXPECT_EQ(mResponse.headers.at("X-Frame-Options"), "DENY");
    EXPECT_EQ(mResponse.headers.at("Referrer-Policy"),
              "strict-origin-when-cross-origin");
    EXPECT_EQ(mResponse.headers.at("Cross-Origin-Opener-Policy"),
              "same-origin");
    EXPECT_EQ(mResponse.headers.at("Cross-Origin-Resource-Policy"),
              "same-origin");
}

TEST_F(SecurityHeadersMiddlewareSpec, DoesNotEmitHstsByDefault)
{
    SecurityHeadersMiddleware mw;
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.count("Strict-Transport-Security"), 0u);
}

TEST_F(SecurityHeadersMiddlewareSpec, EmitsHstsWhenConfigured)
{
    SecurityHeadersOptions opts;
    opts.strictTransportSecurity = "max-age=31536000; includeSubDomains";
    SecurityHeadersMiddleware mw(opts);
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.at("Strict-Transport-Security"),
              "max-age=31536000; includeSubDomains");
}

TEST_F(SecurityHeadersMiddlewareSpec, AllowsOptingOutOfFrameOptions)
{
    SecurityHeadersOptions opts;
    opts.frameOptions = std::nullopt;
    SecurityHeadersMiddleware mw(opts);
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.count("X-Frame-Options"), 0u);
}
