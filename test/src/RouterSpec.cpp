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