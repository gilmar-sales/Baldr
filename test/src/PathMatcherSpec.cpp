#include <gtest/gtest.h>

#include "Baldr/PathMatcher.hpp"

class PathMatcherSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mPathMatcher = std::make_shared<PathMatcher>(); }

    void TearDown() override { mPathMatcher.reset(); }

    Ref<PathMatcher> mPathMatcher;
};

TEST_F(PathMatcherSpec, PathMatcherShouldRegisterGET)
{
    mPathMatcher->insert(
        GET, "/",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    ASSERT_TRUE(mPathMatcher->match(GET, "/").has_value());
}

TEST_F(PathMatcherSpec, PathMatcherShouldRegisterMultipleGETs)
{
    mPathMatcher->insert(
        GET, "/hello_world",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    mPathMatcher->insert(
        GET, "/",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    mPathMatcher->insert(
        GET, "/hello",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    ASSERT_TRUE(mPathMatcher->match(GET, "/hello").has_value());
    ASSERT_TRUE(mPathMatcher->match(GET, "/hello_world").has_value());
    ASSERT_TRUE(mPathMatcher->match(GET, "/").has_value());
}
