#include <Baldr/Http/FromParams.hpp>
#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Router.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
    struct UserPathArgs
    {
        std::string id;
    };

    struct PostFilters
    {
        int limit;
    };
} // namespace

TEST(FromParamsAutoSchema, WithPathTypePopulatesPathParametersJson)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/users/:id")
        .WithPathType<UserPathArgs>()
        .Handle([](baldr::FromParams<UserPathArgs>) -> std::string {
            return "ok";
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("pathParametersJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_FALSE(it->second.empty());
    EXPECT_NE(it->second.find("\"id\""), std::string::npos);
    EXPECT_NE(it->second.find("\"in\":\"path\""), std::string::npos);
    EXPECT_NE(it->second.find("\"required\":true"), std::string::npos);
}

TEST(FromParamsAutoSchema, FromParamsParameterAutoDerivesPathParametersJson)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/users/:id")
        .Handle([](baldr::FromParams<UserPathArgs>) -> std::string {
            return "ok";
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("pathParametersJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"id\""), std::string::npos);
    EXPECT_NE(it->second.find("\"in\":\"path\""), std::string::npos);
}

TEST(FromParamsAutoSchema, MixedFromParamsAndFromQueryBothAutoDerived)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/users/:id/posts")
        .Handle(
            [](baldr::FromParams<UserPathArgs>,
               baldr::FromQuery<PostFilters>) -> std::string { return "ok"; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto pit = entries[0].options.metadata.find("pathParametersJson");
    auto qit = entries[0].options.metadata.find("queryParametersJson");
    ASSERT_NE(pit, entries[0].options.metadata.end());
    ASSERT_NE(qit, entries[0].options.metadata.end());
    EXPECT_NE(pit->second.find("\"id\""), std::string::npos);
    EXPECT_NE(pit->second.find("\"in\":\"path\""), std::string::npos);
    EXPECT_NE(qit->second.find("\"limit\""), std::string::npos);
    EXPECT_NE(qit->second.find("\"in\":\"query\""), std::string::npos);
}
