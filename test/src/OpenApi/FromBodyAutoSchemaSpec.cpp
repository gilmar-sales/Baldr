#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Router.hpp>

#include <gtest/gtest.h>

#include <string>

#include "../Http/UserDto.hpp"

TEST(FromBodyAutoSchema, FromBodyParameterDerivesRequestSchema)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Post, "/login")
        .Handle([](baldr::FromBody<UserDto>) -> std::string { return "ok"; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("requestSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"$ref\":\"#/components/schemas/UserDto\"}");
    EXPECT_TRUE(router.SchemaRegistrySlot()->Contains("UserDto"));
}

TEST(FromBodyAutoSchema, FromBodyDoesNotOverwriteExplicitRequestSchema)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Post, "/login")
        .WithRequestSchemaJson(
            "{\"type\":\"object\",\"description\":\"custom\"}")
        .Handle([](baldr::FromBody<UserDto>) -> std::string { return "ok"; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("requestSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"type\":\"object\",\"description\":\"custom\"}");
    EXPECT_FALSE(router.SchemaRegistrySlot()->Contains("UserDto"));
}

TEST(FromBodyAutoSchema, HandlerWithoutFromBodyLeavesRequestSchemaEmpty)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Post, "/login")
        .Handle([](baldr::HttpRequest&) -> std::string { return "ok"; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].options.metadata.count("requestSchemaJson"), 0u);
}

TEST(FromBodyAutoSchema, FromBodyAlongsideOtherParametersDerivesSchema)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Post, "/echo/:slug")
        .Handle([](baldr::HttpRequest& request, baldr::FromBody<UserDto> body)
                    -> std::string {
            (void) request;
            (void) body;
            return "ok";
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("requestSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"$ref\":\"#/components/schemas/UserDto\"}");
    EXPECT_TRUE(router.SchemaRegistrySlot()->Contains("UserDto"));
}