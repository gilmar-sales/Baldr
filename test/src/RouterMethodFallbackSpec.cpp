#include <Baldr/Http/Router.hpp>

class RouterMethodFallbackSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mRouter = skr::MakeArc<Router>(); }

    skr::Arc<Router> mRouter;
};

TEST_F(RouterMethodFallbackSpec, HeadFallsBackToGetWhenNoHeadRegistered)
{
    mRouter->insert(
        HttpMethod::Get, "/items",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(HttpMethod::Head, "/items");

    ASSERT_TRUE(result.entry.has_value());
    EXPECT_EQ(result.resolvedMethod, HttpMethod::Get);
}

TEST_F(RouterMethodFallbackSpec, HeadPrefersExplicitHeadOverGet)
{
    mRouter->insert(
        HttpMethod::Get, "/items",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(
        HttpMethod::Head, "/items",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(HttpMethod::Head, "/items");

    ASSERT_TRUE(result.entry.has_value());
    EXPECT_EQ(result.resolvedMethod, HttpMethod::Head);
}

TEST_F(RouterMethodFallbackSpec, ReturnsAllowedMethodsWhenPathExistsForOtherMethod)
{
    mRouter->insert(
        HttpMethod::Get, "/items",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(
        HttpMethod::Post, "/items",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(HttpMethod::Delete, "/items");

    EXPECT_FALSE(result.entry.has_value());
    ASSERT_EQ(result.allowedMethodsOnPath.size(), 2u);
    EXPECT_NE(std::find(result.allowedMethodsOnPath.begin(),
                        result.allowedMethodsOnPath.end(),
                        HttpMethod::Get),
              result.allowedMethodsOnPath.end());
    EXPECT_NE(std::find(result.allowedMethodsOnPath.begin(),
                        result.allowedMethodsOnPath.end(),
                        HttpMethod::Post),
              result.allowedMethodsOnPath.end());
}

TEST_F(RouterMethodFallbackSpec, ReturnsEmptyAllowedMethodsWhenPathDoesNotExist)
{
    mRouter->insert(
        HttpMethod::Get, "/items",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(HttpMethod::Delete, "/other");

    EXPECT_FALSE(result.entry.has_value());
    EXPECT_TRUE(result.allowedMethodsOnPath.empty());
}
