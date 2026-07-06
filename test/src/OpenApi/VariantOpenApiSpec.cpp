#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/Results/TypedResults.hpp>
#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Router.hpp>
#include <Baldr/Http/StatusCode.hpp>
#include <Baldr/OpenApi/OpenApiSpecService.hpp>

#include <Skirnir/Skirnir.hpp>

#include <gtest/gtest.h>

#include <simdjson.h>

#include <string>
#include <variant>

#include "../Http/UserDto.hpp"

namespace
{
    struct ValidationError
    {
        std::string field;
        std::string message;
    };

    std::string GetString(simdjson::dom::object& obj, std::string_view key)
    {
        std::string_view sv;
        if (obj[key].get_string().get(sv))
            return {};
        return std::string(sv);
    }
} // namespace

TEST(VariantOpenApi, VariantReturnsEmitOneResponsePerStatus)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/users/:id")
        .Handle([](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                    -> std::variant<baldr::JsonResult, baldr::NotFoundResult> {
            return baldr::Results::NotFound();
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseStatusSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"404\":{\"schema\":{\"type\":\"string\"}}"),
              std::string::npos)
        << "metadata: " << it->second;
}

TEST(VariantOpenApi, VariantWithReflectableSuccessRegistersRef)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/users/:id")
        .Handle([](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                    -> std::variant<UserDto, baldr::NotFoundResult> {
            return baldr::Results::NotFound();
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseStatusSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"200\":{\"$ref\":\"#/components/schemas/"
                              "UserDto\"}"),
              std::string::npos)
        << "metadata: " << it->second;
    EXPECT_NE(it->second.find("\"404\":{\"schema\":{\"type\":\"string\"}}"),
              std::string::npos)
        << "metadata: " << it->second;
    EXPECT_TRUE(router->SchemaRegistrySlot()->Contains("UserDto"));
}

TEST(VariantOpenApi, VariantCombinesWithExplicitWithResponseType)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/users/:id")
        .WithResponseType<UserDto>()
        .Handle([](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                    -> std::variant<UserDto, baldr::NotFoundResult> {
            return baldr::Results::NotFound();
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);

    auto sit = entries[0].options.metadata.find("responseStatusSchemasJson");
    ASSERT_NE(sit, entries[0].options.metadata.end());
    EXPECT_EQ(sit->second, "{\"404\":{\"schema\":{\"type\":\"string\"}}}");

    auto it = entries[0].options.metadata.find("responseSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"$ref\":\"#/components/schemas/UserDto\"}");
}

TEST(VariantOpenApi, VariantRendersMultipleStatusCodesInOpenApi)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/users/:id")
        .WithResponseType<UserDto>()
        .Handle([](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                    -> std::variant<UserDto, baldr::NotFoundResult> {
            return baldr::Results::NotFound();
        });

    baldr::OpenApiOptions     opts;
    baldr::OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(router);
    const std::string& spec = svc.Cached(router);

    simdjson::dom::parser  parser;
    simdjson::dom::element doc;
    ASSERT_FALSE(parser.parse(spec).get(doc));
    simdjson::dom::object root;
    ASSERT_FALSE(doc.get_object().get(root));
    simdjson::dom::object paths;
    ASSERT_FALSE(root["paths"].get_object().get(paths));
    simdjson::dom::object userById;
    ASSERT_FALSE(paths["/users/{id}"].get_object().get(userById));
    simdjson::dom::object getOp;
    ASSERT_FALSE(userById["get"].get_object().get(getOp));
    simdjson::dom::object responses;
    ASSERT_FALSE(getOp["responses"].get_object().get(responses));

    bool saw200 = false;
    bool saw404 = false;
    for (auto kv : responses)
    {
        std::string_view k = kv.key;
        if (k == "200")
        {
            saw200 = true;
            simdjson::dom::object entry;
            ASSERT_FALSE(kv.value.get_object().get(entry));
            simdjson::dom::object content;
            ASSERT_FALSE(entry["content"].get_object().get(content));
            simdjson::dom::object appJson;
            ASSERT_FALSE(content["application/json"].get_object().get(appJson));
            simdjson::dom::object schema;
            ASSERT_FALSE(appJson["schema"].get_object().get(schema));
            std::string_view ref;
            EXPECT_FALSE(schema["$ref"].get_string().get(ref));
            EXPECT_EQ(ref, "#/components/schemas/UserDto");
        }
        else if (k == "404")
        {
            saw404 = true;
            simdjson::dom::object entry;
            ASSERT_FALSE(kv.value.get_object().get(entry));
            simdjson::dom::object content;
            ASSERT_FALSE(entry["content"].get_object().get(content));
            simdjson::dom::object appJson;
            ASSERT_FALSE(content["application/json"].get_object().get(appJson));
            simdjson::dom::object schema;
            ASSERT_FALSE(appJson["schema"].get_object().get(schema));
            std::string_view typeName;
            EXPECT_FALSE(schema["type"].get_string().get(typeName));
            EXPECT_EQ(typeName, "string");
        }
    }
    EXPECT_TRUE(saw200);
    EXPECT_TRUE(saw404);
}

TEST(VariantOpenApi, VariantSkipsLegacyIResultAlternatives)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/legacy/:id")
        .Handle([](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                    -> std::variant<baldr::TextResult, baldr::StatusResult> {
            return baldr::TextResult("hi");
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto sit = entries[0].options.metadata.find("responseStatusSchemasJson");
    EXPECT_EQ(sit, entries[0].options.metadata.end());
}

TEST(VariantOpenApi, ThreeWayVariantProducesThreeStatusEntries)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Put, "/users/:id")
        .Handle([](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                    -> std::variant<baldr::JsonResult,
                                    baldr::BadRequestResult,
                                    baldr::NotFoundResult> {
            return baldr::Results::NotFound();
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseStatusSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"400\""), std::string::npos);
    EXPECT_NE(it->second.find("\"404\""), std::string::npos);
}

TEST(VariantOpenApi, EmptyBodyVariantOmitsContent)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Delete, "/users/:id")
        .Handle(
            [](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                -> std::variant<baldr::NotFoundResult, baldr::NoContentResult> {
                return baldr::NoContentResult();
            });

    baldr::OpenApiOptions     opts;
    baldr::OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(router);
    const std::string& spec = svc.Cached(router);

    simdjson::dom::parser  parser;
    simdjson::dom::element doc;
    ASSERT_FALSE(parser.parse(spec).get(doc));
    simdjson::dom::object root;
    ASSERT_FALSE(doc.get_object().get(root));
    simdjson::dom::object paths;
    ASSERT_FALSE(root["paths"].get_object().get(paths));
    simdjson::dom::object userById;
    ASSERT_FALSE(paths["/users/{id}"].get_object().get(userById));
    simdjson::dom::object delOp;
    ASSERT_FALSE(userById["delete"].get_object().get(delOp));
    simdjson::dom::object responses;
    ASSERT_FALSE(delOp["responses"].get_object().get(responses));

    bool saw204Empty = false;
    for (auto kv : responses)
    {
        std::string_view k = kv.key;
        if (k == "204")
        {
            saw204Empty = true;
            simdjson::dom::object entry;
            ASSERT_FALSE(kv.value.get_object().get(entry));
            simdjson::dom::object content;
            ASSERT_FALSE(entry["content"].get_object().get(content));
            EXPECT_EQ(content.size(), 0u);
        }
    }
    EXPECT_TRUE(saw204Empty);
}

TEST(VariantOpenApi, VariantWithTypedJsonAlternativesEmitsRefs)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Post, "/typed/users")
        .Handle([](baldr::HttpRequest&)
                    -> std::variant<
                        baldr::JsonResult<UserDto, baldr::StatusCode::OK>,
                        baldr::JsonResult<ValidationError,
                                          baldr::StatusCode::BadRequest>> {
            return baldr::Results::Json<UserDto, baldr::StatusCode::OK>(
                UserDto { "ada", 36 });
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseStatusSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"200\":{\"schema\":{\"$ref\":\"#/"
                              "components/schemas/UserDto\"}}"),
              std::string::npos)
        << "metadata: " << it->second;
    EXPECT_NE(it->second.find("\"400\":{\"schema\":{\"$ref\":\"#/"
                              "components/schemas/ValidationError\"}}"),
              std::string::npos)
        << "metadata: " << it->second;
    EXPECT_TRUE(router->SchemaRegistrySlot()->Contains("UserDto"));
    EXPECT_TRUE(router->SchemaRegistrySlot()->Contains("ValidationError"));
}

TEST(VariantOpenApi, VariantTypedJsonSameTypeDifferentStatusRegistersOnce)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router,
                             baldr::HttpMethod::Put,
                             "/typed/users/:id")
        .Handle(
            [](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                -> std::variant<
                    baldr::JsonResult<UserDto, baldr::StatusCode::OK>,
                    baldr::JsonResult<UserDto, baldr::StatusCode::Conflict>> {
                return baldr::Results::Json<UserDto, baldr::StatusCode::OK>(
                    UserDto { "bob", 1 });
            });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseStatusSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"200\":{\"schema\":{\"$ref\":\"#/components/"
                              "schemas/UserDto\"}}"),
              std::string::npos)
        << "metadata: " << it->second;
    EXPECT_NE(it->second.find("\"409\":{\"schema\":{\"$ref\":\"#/components/"
                              "schemas/UserDto\"}}"),
              std::string::npos)
        << "metadata: " << it->second;

    const std::size_t first = it->second.find("UserDto\"");
    EXPECT_NE(first, std::string::npos);
    const std::size_t second = it->second.find("UserDto\"", first + 1);
    EXPECT_NE(second, std::string::npos)
        << "Both statuses should reference UserDto by $ref";
    EXPECT_TRUE(router->SchemaRegistrySlot()->Contains("UserDto"));
}

TEST(VariantOpenApi, VariantTypedJsonConflictResultRenders200And409)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router,
                             baldr::HttpMethod::Patch,
                             "/typed/users/:id")
        .Handle(
            [](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                -> std::variant<
                    baldr::JsonResult<UserDto, baldr::StatusCode::OK>,
                    baldr::JsonResult<UserDto, baldr::StatusCode::Conflict>> {
                return baldr::Results::Json<UserDto,
                                            baldr::StatusCode::Conflict>(
                    UserDto { "carol", 42 });
            });

    baldr::OpenApiOptions     opts;
    baldr::OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(router);
    const std::string& spec = svc.Cached(router);

    simdjson::dom::parser  parser;
    simdjson::dom::element doc;
    ASSERT_FALSE(parser.parse(spec).get(doc));
    simdjson::dom::object root;
    ASSERT_FALSE(doc.get_object().get(root));
    simdjson::dom::object components;
    ASSERT_FALSE(root["components"].get_object().get(components));
    bool sawUserDtoSchema = false;
    {
        simdjson::dom::object schemas;
        ASSERT_FALSE(components["schemas"].get_object().get(schemas));
        for (auto kv : schemas)
        {
            if (std::string_view(kv.key) == "UserDto")
            {
                sawUserDtoSchema = true;
                break;
            }
        }
    }
    EXPECT_TRUE(sawUserDtoSchema);

    simdjson::dom::object paths;
    ASSERT_FALSE(root["paths"].get_object().get(paths));
    simdjson::dom::object userById;
    ASSERT_FALSE(paths["/typed/users/{id}"].get_object().get(userById));
    simdjson::dom::object patchOp;
    ASSERT_FALSE(userById["patch"].get_object().get(patchOp));
    simdjson::dom::object responses;
    ASSERT_FALSE(patchOp["responses"].get_object().get(responses));

    bool saw200 = false;
    bool saw409 = false;
    for (auto kv : responses)
    {
        std::string_view k = kv.key;
        if (k == "200" || k == "409")
        {
            simdjson::dom::object entry;
            ASSERT_FALSE(kv.value.get_object().get(entry));
            simdjson::dom::object content;
            ASSERT_FALSE(entry["content"].get_object().get(content));
            simdjson::dom::object appJson;
            ASSERT_FALSE(content["application/json"].get_object().get(appJson));
            simdjson::dom::object schema;
            ASSERT_FALSE(appJson["schema"].get_object().get(schema));
            std::string_view ref;
            EXPECT_FALSE(schema["$ref"].get_string().get(ref));
            EXPECT_EQ(ref, "#/components/schemas/UserDto");
            if (k == "200")
                saw200 = true;
            else
                saw409 = true;
        }
    }
    EXPECT_TRUE(saw200);
    EXPECT_TRUE(saw409);
}