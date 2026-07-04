#include <Baldr/Http/Router.hpp>

class RouterMethodFallbackSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mRouter = skr::MakeArc<baldr::Router>(); }

    skr::Arc<baldr::Router> mRouter;
};

TEST_F(RouterMethodFallbackSpec, HeadFallsBackToGetWhenNoHeadRegistered)
{
    mRouter->insert(baldr::HttpMethod::Get, "/items",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(baldr::HttpMethod::Head, "/items");

    ASSERT_TRUE(result.entry.has_value());
    EXPECT_EQ(result.resolvedMethod, baldr::HttpMethod::Get);
}

TEST_F(RouterMethodFallbackSpec, HeadPrefersExplicitHeadOverGet)
{
    mRouter->insert(baldr::HttpMethod::Get, "/items",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(baldr::HttpMethod::Head, "/items",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(baldr::HttpMethod::Head, "/items");

    ASSERT_TRUE(result.entry.has_value());
    EXPECT_EQ(result.resolvedMethod, baldr::HttpMethod::Head);
}

TEST_F(RouterMethodFallbackSpec,
       ReturnsAllowedMethodsWhenPathExistsForOtherMethod)
{
    mRouter->insert(baldr::HttpMethod::Get, "/items",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(baldr::HttpMethod::Post, "/items",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(baldr::HttpMethod::Delete, "/items");

    EXPECT_FALSE(result.entry.has_value());
    ASSERT_EQ(result.allowedMethodsOnPath.size(), 2u);
    EXPECT_NE(std::find(result.allowedMethodsOnPath.begin(),
                        result.allowedMethodsOnPath.end(),
                        baldr::HttpMethod::Get),
              result.allowedMethodsOnPath.end());
    EXPECT_NE(std::find(result.allowedMethodsOnPath.begin(),
                        result.allowedMethodsOnPath.end(),
                        baldr::HttpMethod::Post),
              result.allowedMethodsOnPath.end());
}

TEST_F(RouterMethodFallbackSpec, ReturnsEmptyAllowedMethodsWhenPathDoesNotExist)
{
    mRouter->insert(baldr::HttpMethod::Get, "/items",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(baldr::HttpMethod::Delete, "/other");

    EXPECT_FALSE(result.entry.has_value());
    EXPECT_TRUE(result.allowedMethodsOnPath.empty());
}
