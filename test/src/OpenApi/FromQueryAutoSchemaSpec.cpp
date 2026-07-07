#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Router.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
    struct SearchFilters
    {
        std::string name;
        int         age;
        bool        active;
    };
} // namespace

TEST(FromQueryAutoSchema, WithQueryTypePopulatesQueryParametersJson)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/search")
        .WithQueryType<SearchFilters>()
        .Handle([](baldr::FromQuery<SearchFilters>) -> std::string {
            return "ok";
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("queryParametersJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_FALSE(it->second.empty());
    EXPECT_NE(it->second.find("\"name\""), std::string::npos);
    EXPECT_NE(it->second.find("\"age\""), std::string::npos);
    EXPECT_NE(it->second.find("\"active\""), std::string::npos);
    EXPECT_NE(it->second.find("\"in\":\"query\""), std::string::npos);
    EXPECT_NE(it->second.find("\"required\":true"), std::string::npos);
}

TEST(FromQueryAutoSchema, FromQueryParameterAutoDerivesParametersJson)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/search")
        .Handle([](baldr::FromQuery<SearchFilters>) -> std::string {
            return "ok";
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("queryParametersJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"name\""), std::string::npos);
    EXPECT_NE(it->second.find("\"in\":\"query\""), std::string::npos);
}

TEST(FromQueryAutoSchema, ExplicitWithQueryTypeOverridesAutoDerive)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/search")
        .WithQueryType<SearchFilters>()
        .Handle([](baldr::FromQuery<SearchFilters>) -> std::string {
            return "ok";
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("queryParametersJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"name\""), std::string::npos);
}
