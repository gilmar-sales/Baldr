#include <Baldr/Middleware/SecurityHeaders.hpp>

#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>

class SecurityHeadersMiddlewareSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRequest.method  = baldr::HttpMethod::Get;
        mRequest.path    = "/";
        mRequest.version = "HTTP/1.1";
        mResponse        = baldr::HttpResponse(mRequest);
    }

    baldr::HttpRequest  mRequest;
    baldr::HttpResponse mResponse;
};

TEST_F(SecurityHeadersMiddlewareSpec, AppliesDefaultHeaders)
{
    baldr::SecurityHeadersMiddleware mw;
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
    baldr::SecurityHeadersMiddleware mw;
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.count("Strict-Transport-Security"), 0u);
}

TEST_F(SecurityHeadersMiddlewareSpec, EmitsHstsWhenConfigured)
{
    baldr::SecurityHeadersOptions opts;
    opts.strictTransportSecurity = "max-age=31536000; includeSubDomains";
    baldr::SecurityHeadersMiddleware mw(opts);
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.at("Strict-Transport-Security"),
              "max-age=31536000; includeSubDomains");
}

TEST_F(SecurityHeadersMiddlewareSpec, AllowsOptingOutOfFrameOptions)
{
    baldr::SecurityHeadersOptions opts;
    opts.frameOptions = std::nullopt;
    baldr::SecurityHeadersMiddleware mw(opts);
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.count("X-Frame-Options"), 0u);
}

TEST_F(SecurityHeadersMiddlewareSpec, StripsCrlfFromConfiguredValues)
{
    baldr::SecurityHeadersOptions opts;
    opts.frameOptions            = "DENY\r\nSet-Cookie: pwn=1";
    opts.contentTypeOptions      = std::string("nosniff\nX-Inject: 1");
    opts.strictTransportSecurity = std::string("max-age=1\r\nEvil: yes");
    baldr::SecurityHeadersMiddleware mw(std::move(opts));
    mw.Handle(mRequest, mResponse, []() {});

    auto containsCrlf = [](const std::string& v) {
        return v.find('\r') != std::string::npos ||
               v.find('\n') != std::string::npos;
    };
    for (const auto& [k, v] : mResponse.headers)
    {
        EXPECT_FALSE(containsCrlf(v)) << "header " << k << " still has CR/LF";
    }
    EXPECT_EQ(mResponse.headers.at("X-Frame-Options"),
              "DENY Set-Cookie: pwn=1");
    EXPECT_EQ(mResponse.headers.at("X-Content-Type-Options"),
              "nosniff X-Inject: 1");
    EXPECT_EQ(mResponse.headers.at("Strict-Transport-Security"),
              "max-age=1 Evil: yes");
}

TEST_F(SecurityHeadersMiddlewareSpec, DropsAllSpacesFromValueThatIsOnlyCrlf)
{
    baldr::SecurityHeadersOptions opts;
    opts.frameOptions = std::string("\r\n\r\n");
    baldr::SecurityHeadersMiddleware mw(std::move(opts));
    mw.Handle(mRequest, mResponse, []() {});

    EXPECT_EQ(mResponse.headers.count("X-Frame-Options"), 0u);
}
