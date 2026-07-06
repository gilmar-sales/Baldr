#include <Baldr/Http/RouteGuard.hpp>
#include <Baldr/Http/RouteOptions.hpp>

#include <gtest/gtest.h>

#include <string>

class RouteGuardSpec : public ::testing::Test
{
};

TEST_F(RouteGuardSpec, ReturnsNulloptWhenNoOverride)
{
    baldr::RouteOptions opts;
    baldr::HttpRequest  req;
    EXPECT_FALSE(baldr::EnforceMaxBodySize(req, opts).has_value());
}

TEST_F(RouteGuardSpec, ReturnsNulloptWhenBodyWithinCap)
{
    baldr::RouteOptions opts;
    opts.maxBodyBytes = 16;

    baldr::HttpRequest req;
    req.body = std::string(8, 'x');
    EXPECT_FALSE(baldr::EnforceMaxBodySize(req, opts).has_value());
}

TEST_F(RouteGuardSpec, Returns413WhenAccumulatedBodyExceedsCap)
{
    baldr::RouteOptions opts;
    opts.maxBodyBytes = 4;

    baldr::HttpRequest req;
    req.body  = std::string(10, 'x');
    auto resp = baldr::EnforceMaxBodySize(req, opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(static_cast<int>(resp->statusCode),
              static_cast<int>(baldr::StatusCode::PayloadTooLarge));
    EXPECT_EQ(resp->body, "Payload Too Large");
    EXPECT_EQ(resp->headers.at("Content-Type"), "text/plain");
    EXPECT_EQ(resp->headers.at("Connection"), "close");
}

TEST_F(RouteGuardSpec, Returns413WhenDeclaredContentLengthExceedsCap)
{
    baldr::RouteOptions opts;
    opts.maxBodyBytes = 16;

    baldr::HttpRequest req;
    req.body                      = std::string(4, 'x');
    req.headers["content-length"] = "9999";
    auto resp                     = baldr::EnforceMaxBodySize(req, opts);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(static_cast<int>(resp->statusCode),
              static_cast<int>(baldr::StatusCode::PayloadTooLarge));
}

TEST_F(RouteGuardSpec, IgnoresMalformedContentLengthHeader)
{
    baldr::RouteOptions opts;
    opts.maxBodyBytes = 16;

    baldr::HttpRequest req;
    req.body                      = std::string(4, 'x');
    req.headers["content-length"] = "not-a-number";
    EXPECT_FALSE(baldr::EnforceMaxBodySize(req, opts).has_value());
}

TEST_F(RouteGuardSpec, HonorsExactBoundaryAsAllowed)
{
    baldr::RouteOptions opts;
    opts.maxBodyBytes = 8;

    baldr::HttpRequest req;
    req.body = std::string(8, 'x');
    EXPECT_FALSE(baldr::EnforceMaxBodySize(req, opts).has_value());
}