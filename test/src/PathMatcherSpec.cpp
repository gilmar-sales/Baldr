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
        "GET", "/",
        [](HttpRequest&, HttpResponse&, Ref<skr::ServiceProvider>) {});

    ASSERT_TRUE(mPathMatcher->match("GET", "/").has_value());
}