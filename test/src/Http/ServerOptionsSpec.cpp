#include <Baldr/Http/ServerOptions.hpp>

TEST(HttpServerOptionsTest, DefaultKeepAlivePolicyIsHttp11KeepAlive)
{
    HttpServerOptions opts;
    EXPECT_TRUE(opts.enableHttp11KeepAlive);
    EXPECT_EQ(opts.maxRequestsPerConnection, 1000);
}

TEST(HttpServerOptionsTest, CanDisableKeepAlive)
{
    HttpServerOptions opts;
    opts.enableHttp11KeepAlive  = false;
    opts.maxRequestsPerConnection = 0;

    EXPECT_FALSE(opts.enableHttp11KeepAlive);
    EXPECT_EQ(opts.maxRequestsPerConnection, 0);
}
