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

struct IdArg
{
    std::string id;
};

TEST(TypedResults, OkResultHasExpectedStatus)
{
    baldr::OkResult r("hello");
    EXPECT_EQ(r.StatusFor(), baldr::StatusCode::OK);
    EXPECT_EQ(r.ContentTypeFor(), "text/plain");
}

TEST(TypedResults, NotFoundResultHasExpectedStatus)
{
    baldr::NotFoundResult r;
    EXPECT_EQ(r.StatusFor(), baldr::StatusCode::NotFound);
    EXPECT_EQ(r.ContentTypeFor(), "text/plain");
}

TEST(TypedResults, NoContentResultHasEmptySchema)
{
    baldr::NoContentResult r;
    EXPECT_EQ(r.StatusFor(), baldr::StatusCode::NoContent);
    EXPECT_EQ(r.ContentTypeFor(), "");
    EXPECT_EQ(r.SchemaJsonFor(), "{}");
}

TEST(TypedResults, ApplyWritesStatusCode)
{
    baldr::HttpResponse   response;
    baldr::NotFoundResult r("missing");
    r.Apply(response);
    EXPECT_EQ(response.statusCode, baldr::StatusCode::NotFound);
    EXPECT_EQ(response.body, "missing");
    EXPECT_EQ(response.headers.at("Content-Type"), "text/plain");
}

TEST(TypedResults, HandleDerivesPerStatusMetadata)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/users/:id")
        .Handle([](baldr::HttpRequest&,
                   baldr::FromParams<IdArg>) -> baldr::NotFoundResult {
            return baldr::Results::NotFound();
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"404\""), std::string::npos);
    EXPECT_NE(it->second.find("\"schema\":"), std::string::npos);
}

TEST(TypedResults, OpenApiRendersMultipleStatusCodes)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/users/:id")
        .Handle([](baldr::HttpRequest&,
                   baldr::FromParams<IdArg>) -> baldr::NotFoundResult {
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

    bool saw404 = false;
    for (auto kv : responses)
    {
        std::string_view k = kv.key;
        if (k == "404")
        {
            saw404 = true;
            simdjson::dom::object entry;
            ASSERT_FALSE(kv.value.get_object().get(entry));
            std::string_view desc;
            EXPECT_FALSE(entry["description"].get_string().get(desc));
            EXPECT_EQ(desc, "404");
            simdjson::dom::object content;
            EXPECT_FALSE(entry["content"].get_object().get(content));
            simdjson::dom::object appJson;
            ASSERT_FALSE(content["application/json"].get_object().get(appJson));
            simdjson::dom::object schema;
            ASSERT_FALSE(appJson["schema"].get_object().get(schema));
            std::string_view typeName;
            EXPECT_FALSE(schema["type"].get_string().get(typeName));
            EXPECT_EQ(typeName, "string");
        }
    }
    EXPECT_TRUE(saw404);
}

TEST(TypedResults, EmptyBodyResultOmitsContent)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Delete, "/users/:id")
        .Handle([](baldr::HttpRequest&,
                   baldr::FromParams<IdArg>) -> baldr::NoContentResult {
            return baldr::NoContentResult();
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"204\":{\"schema\":{}}}");
}

TEST(TypedResults, TypedResultDoesNotOverwriteExplicitSchema)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/users/:id")
        .WithResponseSchemaJson("{\"$ref\":\"#/components/schemas/User\"}")
        .Handle([](baldr::HttpRequest&) -> baldr::OkResult {
            return baldr::OkResult("hi");
        });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto sit = entries[0].options.metadata.find("responseSchemasJson");
    EXPECT_EQ(sit, entries[0].options.metadata.end());
    auto it = entries[0].options.metadata.find("responseSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"$ref\":\"#/components/schemas/User\"}");
}

TEST(TypedResults, VariantReturnProducesPerStatusMetadata)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Delete, "/users/:id")
        .Handle(
            [](baldr::HttpRequest&, baldr::FromParams<IdArg>)
                -> std::variant<baldr::NotFoundResult, baldr::NoContentResult> {
                return baldr::NoContentResult();
            });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseStatusSchemasJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_NE(it->second.find("\"404\""), std::string::npos);
    EXPECT_NE(it->second.find("\"204\""), std::string::npos);
}