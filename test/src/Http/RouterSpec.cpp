#include <Baldr/Http/Router.hpp>

class RouterSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mRouter = skr::MakeArc<baldr::Router>(); }

    void TearDown() override { mRouter.reset(); }

    skr::Arc<baldr::Router> mRouter;
};

TEST_F(RouterSpec, RouterShouldRegisterGET)
{
    // Arrange
    const auto method = baldr::HttpMethod::Get;
    const auto path   = "/hello_world";

    // Act
    mRouter->insert(method, path,
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    // Assert
    ASSERT_TRUE(
        mRouter->match(baldr::HttpMethod::Get, "/hello_world").has_value());
}

TEST_F(RouterSpec, RouterShouldMatchPrefixedRoutes)
{
    // Simulates the path that MapGroup("/api/v1", ...) produces.
    mRouter->insert(baldr::HttpMethod::Get, "/api/v1/users",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(baldr::HttpMethod::Get, "/api/v1/orders/:id",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    ASSERT_TRUE(
        mRouter->match(baldr::HttpMethod::Get, "/api/v1/users").has_value());
    auto orderMatch =
        mRouter->match(baldr::HttpMethod::Get, "/api/v1/orders/42");
    ASSERT_TRUE(orderMatch.has_value());
    auto params = orderMatch->extractRouteParams("/api/v1/orders/42");
    EXPECT_EQ(params.at("id"), "42");
}

TEST_F(RouterSpec, RouterShouldRegisterMultipleGETs)
{
    // Arrange
    const auto method = baldr::HttpMethod::Get;
    const auto paths =
        std::vector<std::string> { "/hello_world", "/", "/hello" };

    // Act
    for (const auto& path : paths)
    {
        mRouter->insert(method, path,
                        [](baldr::HttpRequest&, baldr::HttpResponse&,
                           skr::Arc<skr::ServiceProvider>) {});
    }

    // Assert
    ASSERT_TRUE(mRouter->match(baldr::HttpMethod::Get, "/hello").has_value());
    ASSERT_TRUE(
        mRouter->match(baldr::HttpMethod::Get, "/hello_world").has_value());
    ASSERT_TRUE(mRouter->match(baldr::HttpMethod::Get, "/").has_value());
}

TEST_F(RouterSpec, RouterShoulNotConflictMethods)
{
    // Arrange
    const auto method = baldr::HttpMethod::Get;
    const auto path   = "/";

    // Act
    mRouter->insert(baldr::HttpMethod::Get, "/",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    // Assert
    ASSERT_FALSE(mRouter->match(baldr::HttpMethod::Post, "/").has_value());
    ASSERT_FALSE(mRouter->match(baldr::HttpMethod::Delete, "/").has_value());
    ASSERT_FALSE(mRouter->match(baldr::HttpMethod::Put, "/").has_value());
}

TEST_F(RouterSpec, RouterShouldExtractSingleRouteParam)
{
    mRouter->insert(baldr::HttpMethod::Get, "/hello/:name",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto routeEntry =
        mRouter->match(baldr::HttpMethod::Get, "/hello/Lorem Impsum");

    ASSERT_TRUE(routeEntry.has_value());
    ASSERT_STREQ(
        routeEntry->extractRouteParams("/hello/Lorem Impsum")["name"].c_str(),
        "Lorem Impsum");
}

TEST_F(RouterSpec, RouterShouldExtractMultipleRouteParams)
{
    mRouter->insert(baldr::HttpMethod::Get, "/hello/:name/:surname",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto routeEntry =
        mRouter->match(baldr::HttpMethod::Get, "/hello/Lorem/Impsum");

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
    mRouter->insert(baldr::HttpMethod::Get, "/admin",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    // POST to same path should NOT match
    ASSERT_FALSE(mRouter->match(baldr::HttpMethod::Post, "/admin").has_value());
    ASSERT_FALSE(
        mRouter->match(baldr::HttpMethod::Delete, "/admin").has_value());
    ASSERT_FALSE(mRouter->match(baldr::HttpMethod::Put, "/admin").has_value());
}

TEST_F(RouterSpec, RouterShouldNotMatchUnregisteredPaths)
{
    // Register /admin
    mRouter->insert(baldr::HttpMethod::Get, "/admin",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    // Similar but different path should NOT match
    ASSERT_FALSE(
        mRouter->match(baldr::HttpMethod::Get, "/admin123").has_value());
    ASSERT_FALSE(mRouter->match(baldr::HttpMethod::Get, "/admins").has_value());
    ASSERT_FALSE(mRouter->match(baldr::HttpMethod::Get, "/").has_value());
}

// ============================================================================
// CWE-835: Infinite Loop Prevention (Regex DoS)
// ============================================================================

TEST_F(RouterSpec, RouterWithManyRouteParametersShouldNotCauseReDoS)
{
    // Pattern like /(:a)*/ can cause catastrophic backtracking
    // The current implementation uses [^/]+ which is safe

    mRouter->insert(baldr::HttpMethod::Get, "/user/:id/profile/:name",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    // Should match quickly without ReDoS
    auto start = std::chrono::high_resolution_clock::now();
    auto match =
        mRouter->match(baldr::HttpMethod::Get, "/user/123/profile/john");
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(match.has_value());

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    // Should complete in well under 1 second
    ASSERT_LT(duration.count(), 100000);
}

TEST_F(RouterSpec, RouterShouldHandleDeeplyNestedPaths)
{
    // Deep path with many segments
    mRouter->insert(baldr::HttpMethod::Get, "/a/b/c/d/e/f/g/h/i/j",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto result =
        mRouter->match(baldr::HttpMethod::Get, "/a/b/c/d/e/f/g/h/i/j");

    ASSERT_TRUE(result.has_value());
}

// ============================================================================
// Multi-segment literal paths (request/response behaviour parity)
// ============================================================================
//
// These tests pin down the matching behaviour for the literal, multi-segment
// routes used by the example applications (e.g. Devices exposes
// GET /api/devices). They guarantee that registering a literal path returns
// a route entry and that a request with the same path resolves to it.

TEST_F(RouterSpec, RouterShouldMatchLiteralMultiSegmentPath)
{
    mRouter->insert(baldr::HttpMethod::Get, "/api/devices",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto matched = mRouter->match(baldr::HttpMethod::Get, "/api/devices");

    ASSERT_TRUE(matched.has_value());
    EXPECT_FALSE(matched->hasParams);
    EXPECT_TRUE(matched->paramsNames.empty());
}

TEST_F(RouterSpec, RouterMatchesLiteralMultiSegmentPathWithTrailingSlash)
{
    // The router inserts each segment with an optional trailing slash, so
    // registering "/api/devices" also matches "/api/devices/". This is
    // intentional behaviour shared by the trie traversal and the param
    // extraction regex (Router::insert builds `^/api/?devices/?$`).
    // Pin it so that any future change to that normalization is a
    // deliberate, reviewed decision rather than a silent regression.
    mRouter->insert(baldr::HttpMethod::Get, "/api/devices",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    EXPECT_TRUE(
        mRouter->match(baldr::HttpMethod::Get, "/api/devices").has_value());
    EXPECT_TRUE(
        mRouter->match(baldr::HttpMethod::Get, "/api/devices/").has_value());
}

TEST_F(RouterSpec, RouterShouldNotMatchSuperstringOfLiteralPath)
{
    mRouter->insert(baldr::HttpMethod::Get, "/api/devices",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    EXPECT_FALSE(mRouter->match(baldr::HttpMethod::Get, "/api/devicesExtra")
                     .has_value());
    EXPECT_FALSE(
        mRouter->match(baldr::HttpMethod::Get, "/api/devices/v2").has_value());
}

TEST_F(RouterSpec, RouterShouldPreferExactMatchOverParametricMatchForSamePath)
{
    // Insert parametric first; then a literal that is a prefix of a
    // parametric match candidate. Both must remain individually
    // addressable.
    mRouter->insert(baldr::HttpMethod::Get, "/api/devices/:id",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(baldr::HttpMethod::Get, "/api/devices",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto literal = mRouter->match(baldr::HttpMethod::Get, "/api/devices");
    auto byId    = mRouter->match(baldr::HttpMethod::Get, "/api/devices/42");

    ASSERT_TRUE(literal.has_value());
    ASSERT_TRUE(byId.has_value());

    EXPECT_FALSE(literal->hasParams);

    ASSERT_TRUE(byId->hasParams);
    EXPECT_STREQ(byId->extractRouteParams("/api/devices/42")["id"].c_str(),
                 "42");
}

TEST_F(RouterSpec, RouterShouldExtractParamFromLiteralMultiSegmentContext)
{
    mRouter->insert(baldr::HttpMethod::Get, "/api/devices/:id",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto matched = mRouter->match(baldr::HttpMethod::Get, "/api/devices/abc");

    ASSERT_TRUE(matched.has_value());
    EXPECT_STREQ(matched->extractRouteParams("/api/devices/abc")["id"].c_str(),
                 "abc");
}

// ============================================================================
// Greedy catch-all (`**`) — multi-segment static-files support.
// ============================================================================

TEST_F(RouterSpec, RouterMatchesGreedyCatchAll)
{
    mRouter->insert(baldr::HttpMethod::Get, "/static/**",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    EXPECT_TRUE(mRouter->match(baldr::HttpMethod::Get, "/static").has_value());
    EXPECT_TRUE(mRouter->match(baldr::HttpMethod::Get, "/static/").has_value());
    EXPECT_TRUE(
        mRouter->match(baldr::HttpMethod::Get, "/static/a").has_value());
    EXPECT_TRUE(mRouter->match(baldr::HttpMethod::Get, "/static/css/site.css")
                    .has_value());
}

TEST_F(RouterSpec, RouterGreedyCatchAllCapturesRemainderWithoutLeadingSlash)
{
    mRouter->insert(baldr::HttpMethod::Get, "/static/**",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto one = mRouter->match(baldr::HttpMethod::Get, "/static/a");
    ASSERT_TRUE(one.has_value());
    EXPECT_STREQ(one->extractRouteParams("/static/a")["filepath"].c_str(), "a");

    auto many = mRouter->match(baldr::HttpMethod::Get, "/static/css/site.css");
    ASSERT_TRUE(many.has_value());
    EXPECT_STREQ(
        many->extractRouteParams("/static/css/site.css")["filepath"].c_str(),
        "css/site.css");

    auto bare = mRouter->match(baldr::HttpMethod::Get, "/static");
    ASSERT_TRUE(bare.has_value());
    EXPECT_TRUE(bare->extractRouteParams("/static")["filepath"].empty());

    auto trailing = mRouter->match(baldr::HttpMethod::Get, "/static/");
    ASSERT_TRUE(trailing.has_value());
    EXPECT_TRUE(trailing->extractRouteParams("/static/")["filepath"].empty());
}

TEST_F(RouterSpec, RouterRejectsGreedyCatchAllNotTerminal)
{
    EXPECT_THROW(mRouter->insert(baldr::HttpMethod::Get, "/static/**/x",
                                 [](baldr::HttpRequest&, baldr::HttpResponse&,
                                    skr::Arc<skr::ServiceProvider>) {}),
                 std::invalid_argument);

    EXPECT_THROW(mRouter->insert(baldr::HttpMethod::Get, "/static/**/:name",
                                 [](baldr::HttpRequest&, baldr::HttpResponse&,
                                    skr::Arc<skr::ServiceProvider>) {}),
                 std::invalid_argument);
}

TEST_F(RouterSpec, RouterLiteralSegmentTakesPrecedenceOverGreedyCatchAll)
{
    mRouter->insert(baldr::HttpMethod::Get, "/static/**",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(baldr::HttpMethod::Get, "/static/index",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto exact = mRouter->match(baldr::HttpMethod::Get, "/static/index");
    ASSERT_TRUE(exact.has_value());
    EXPECT_FALSE(exact->hasParams);

    auto nested =
        mRouter->match(baldr::HttpMethod::Get, "/static/index/nested");
    ASSERT_TRUE(nested.has_value());
    EXPECT_STREQ(
        nested->extractRouteParams("/static/index/nested")["filepath"].c_str(),
        "index/nested");
}

TEST_F(RouterSpec, RouterGreedyCatchAllIsIsolatedToItsMethod)
{
    mRouter->insert(baldr::HttpMethod::Get, "/static/**",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    EXPECT_TRUE(
        mRouter->match(baldr::HttpMethod::Get, "/static/a/b").has_value());
    EXPECT_FALSE(
        mRouter->match(baldr::HttpMethod::Post, "/static/a/b").has_value());
    EXPECT_FALSE(
        mRouter->match(baldr::HttpMethod::Delete, "/static/a").has_value());
}
