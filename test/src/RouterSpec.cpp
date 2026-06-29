#include "Baldr/Router.hpp"

class RouterSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mRouter = skr::MakeArc<Router>(); }

    void TearDown() override { mRouter.reset(); }

    skr::Arc<Router> mRouter;
};

TEST_F(RouterSpec, RouterShouldRegisterGET)
{
    // Arrange
    const auto method = HttpMethod::Get;
    const auto path   = "/hello_world";

    // Act
    mRouter->insert(
        method, path,
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    // Assert
    ASSERT_TRUE(mRouter->match(HttpMethod::Get, "/hello_world").has_value());
}

TEST_F(RouterSpec, RouterShouldRegisterMultipleGETs)
{
    // Arrange
    const auto method = HttpMethod::Get;
    const auto paths =
        std::vector<std::string> { "/hello_world", "/", "/hello" };

    // Act
    for (const auto& path : paths)
    {
        mRouter->insert(
            method, path,
            [](HttpRequest&, HttpResponse&,
               skr::Arc<skr::ServiceProvider>) {});
    }

    // Assert
    ASSERT_TRUE(mRouter->match(HttpMethod::Get, "/hello").has_value());
    ASSERT_TRUE(mRouter->match(HttpMethod::Get, "/hello_world").has_value());
    ASSERT_TRUE(mRouter->match(HttpMethod::Get, "/").has_value());
}

TEST_F(RouterSpec, RouterShoulNotConflictMethods)
{
    // Arrange
    const auto method = HttpMethod::Get;
    const auto path   = "/";

    // Act
    mRouter->insert(
        HttpMethod::Get, "/",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    // Assert
    ASSERT_FALSE(mRouter->match(HttpMethod::Post, "/").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::Delete, "/").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::Put, "/").has_value());
}

TEST_F(RouterSpec, RouterShouldExtractSingleRouteParam)
{
    mRouter->insert(
        HttpMethod::Get, "/hello/:name",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    auto routeEntry = mRouter->match(HttpMethod::Get, "/hello/Lorem Impsum");

    ASSERT_TRUE(routeEntry.has_value());
    ASSERT_STREQ(
        routeEntry->extractRouteParams("/hello/Lorem Impsum")["name"].c_str(),
        "Lorem Impsum");
}

TEST_F(RouterSpec, RouterShouldExtractMultipleRouteParams)
{
    mRouter->insert(
        HttpMethod::Get, "/hello/:name/:surname",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    auto routeEntry = mRouter->match(HttpMethod::Get, "/hello/Lorem/Impsum");

    ASSERT_TRUE(routeEntry.has_value());
    ASSERT_STREQ(
        routeEntry->extractRouteParams("/hello/Lorem/Impsum")["name"].c_str(),
        "Lorem");
    ASSERT_STREQ(
        routeEntry->extractRouteParams("/hello/Lorem/Impsum")["surname"]
            .c_str(),
        "Impsum");
}

TEST_F(RouterSpec, RouterShouldNotMatchDifferentHTTPMethods)
{
    // Register only GET
    mRouter->insert(
        HttpMethod::Get, "/admin",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    // POST to same path should NOT match
    ASSERT_FALSE(mRouter->match(HttpMethod::Post, "/admin").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::Delete, "/admin").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::Put, "/admin").has_value());
}

TEST_F(RouterSpec, RouterShouldNotMatchUnregisteredPaths)
{
    // Register /admin
    mRouter->insert(
        HttpMethod::Get, "/admin",
        [](HttpRequest&, HttpResponse&,
           skr::Arc<skr::ServiceProvider>) {});

    // Similar but different path should NOT match
    ASSERT_FALSE(mRouter->match(HttpMethod::Get, "/admin123").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::Get, "/admins").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::Get, "/").has_value());
}

// ============================================================================
// CWE-835: Infinite Loop Prevention (Regex DoS)
// ============================================================================

TEST_F(RouterSpec, RouterWithManyRouteParametersShouldNotCauseReDoS)
{
    // Pattern like /(:a)*/ can cause catastrophic backtracking
    // The current implementation uses [^/]+ which is safe

    mRouter->insert(
        HttpMethod::Get, "/user/:id/profile/:name",
        [](HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>) {});

    // Should match quickly without ReDoS
    auto start = std::chrono::high_resolution_clock::now();
    auto match = mRouter->match(HttpMethod::Get, "/user/123/profile/john");
    auto end   = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(match.has_value());

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // Should complete in well under 1 second
    ASSERT_LT(duration.count(), 100000);
}

TEST_F(RouterSpec, RouterShouldHandleDeeplyNestedPaths)
{
    // Deep path with many segments
    mRouter->insert(
        HttpMethod::Get, "/a/b/c/d/e/f/g/h/i/j",
        [](HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->match(HttpMethod::Get, "/a/b/c/d/e/f/g/h/i/j");

    ASSERT_TRUE(result.has_value());
}
