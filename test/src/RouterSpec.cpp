#include "Baldr/Router.hpp"

class RouterSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mRouter = skr::MakeRef<Router>(); }

    void TearDown() override { mRouter.reset(); }

    Ref<Router> mRouter;
};

TEST_F(RouterSpec, RouterShouldRegisterGET)
{
    // Arrange
    const auto method = HttpMethod::GET;
    const auto path   = "/hello_world";

    // Act
    mRouter->insert(
        method, path,
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    // Assert
    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/hello_world").has_value());
}

TEST_F(RouterSpec, RouterShouldRegisterMultipleGETs)
{
    // Arrange
    const auto method = HttpMethod::GET;
    const auto paths =
        std::vector<std::string> { "/hello_world", "/", "/hello" };

    // Act
    for (const auto& path : paths)
    {
        mRouter->insert(
            method, path,
            [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});
    }

    // Assert
    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/hello").has_value());
    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/hello_world").has_value());
    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/").has_value());
}

TEST_F(RouterSpec, RouterShoulNotConflictMethods)
{
    // Arrange
    const auto method = HttpMethod::GET;
    const auto path   = "/";

    // Act
    mRouter->insert(
        HttpMethod::GET, "/",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    // Assert
    ASSERT_FALSE(mRouter->match(HttpMethod::POST, "/").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::DELETE, "/").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::PUT, "/").has_value());
}

TEST_F(RouterSpec, RouterShouldExtractSingleRouteParam)
{
    mRouter->insert(
        HttpMethod::GET, "/hello/:name",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    auto routeEntry = mRouter->match(HttpMethod::GET, "/hello/Lorem Impsum");

    ASSERT_TRUE(routeEntry.has_value());
    ASSERT_STREQ(
        routeEntry->extractRouteParams("/hello/Lorem Impsum")["name"].c_str(),
        "Lorem Impsum");
}

TEST_F(RouterSpec, RouterShouldExtractMultipleRouteParams)
{
    mRouter->insert(
        HttpMethod::GET, "/hello/:name/:surname",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    auto routeEntry = mRouter->match(HttpMethod::GET, "/hello/Lorem/Impsum");

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
        HttpMethod::GET, "/admin",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    // POST to same path should NOT match
    ASSERT_FALSE(mRouter->match(HttpMethod::POST, "/admin").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::DELETE, "/admin").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::PUT, "/admin").has_value());
}

TEST_F(RouterSpec, RouterShouldNotMatchUnregisteredPaths)
{
    // Register /admin
    mRouter->insert(
        HttpMethod::GET, "/admin",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    // Similar but different path should NOT match
    ASSERT_FALSE(mRouter->match(HttpMethod::GET, "/admin123").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::GET, "/admins").has_value());
    ASSERT_FALSE(mRouter->match(HttpMethod::GET, "/").has_value());
}

// ============================================================================
// CWE-835: Infinite Loop Prevention (Regex DoS)
// ============================================================================

TEST_F(RouterSpec, RouterWithManyRouteParametersShouldNotCauseReDoS)
{
    // Pattern like /(:a)*/ can cause catastrophic backtracking
    // The current implementation uses [^/]+ which is safe

    mRouter->insert(
        HttpMethod::GET, "/user/:id/profile/:name",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    // Should match quickly without ReDoS
    auto start = std::chrono::high_resolution_clock::now();
    auto match = mRouter->match(HttpMethod::GET, "/user/123/profile/john");
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
        HttpMethod::GET, "/a/b/c/d/e/f/g/h/i/j",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    auto result = mRouter->match(HttpMethod::GET, "/a/b/c/d/e/f/g/h/i/j");

    ASSERT_TRUE(result.has_value());
}
