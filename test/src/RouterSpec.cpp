#include <gtest/gtest.h>

#include "Baldr/Router.hpp"

class RouterSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mRouter = std::make_shared<Router>(); }

    void TearDown() override { mRouter.reset(); }

    Ref<Router> mRouter;
};

TEST_F(RouterSpec, RouterShouldRegisterGET)
{
    mRouter->insert(
        HttpMethod::GET, "/",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/").has_value());
}

TEST_F(RouterSpec, RouterShouldRegisterMultipleGETs)
{
    mRouter->insert(
        HttpMethod::GET, "/hello_world",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    mRouter->insert(
        HttpMethod::GET, "/",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    mRouter->insert(
        HttpMethod::GET, "/hello",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/hello").has_value());
    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/hello_world").has_value());
    ASSERT_TRUE(mRouter->match(HttpMethod::GET, "/").has_value());
}
