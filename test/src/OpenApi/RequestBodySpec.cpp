#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Router.hpp>
#include <Baldr/OpenApi/OpenApiSpecService.hpp>

#include <Skirnir/Skirnir.hpp>

#include <gtest/gtest.h>

#include <simdjson.h>

#include <string>

#include "../Http/UserDto.hpp"

TEST(RequestBody, FromBodyProducesRequestBodyInSpec)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Post, "/login")
        .Handle([](baldr::FromBody<UserDto>) -> std::string { return "ok"; });

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
    simdjson::dom::object login;
    ASSERT_FALSE(paths["/login"].get_object().get(login));
    simdjson::dom::object postOp;
    ASSERT_FALSE(login["post"].get_object().get(postOp));
    simdjson::dom::object requestBody;
    ASSERT_FALSE(postOp["requestBody"].get_object().get(requestBody));

    bool                   requiredPresent = false;
    simdjson::dom::element requiredEl;
    if (requestBody["required"].get(requiredEl) == simdjson::SUCCESS)
    {
        requiredPresent  = true;
        bool requiredVal = false;
        EXPECT_FALSE(requiredEl.get_bool().get(requiredVal));
        EXPECT_FALSE(requiredVal);
    }
    EXPECT_TRUE(requiredPresent);

    simdjson::dom::object content;
    ASSERT_FALSE(requestBody["content"].get_object().get(content));
    simdjson::dom::object json;
    ASSERT_FALSE(content["application/json"].get_object().get(json));
    simdjson::dom::object schema;
    ASSERT_FALSE(json["schema"].get_object().get(schema));
    std::string_view ref;
    ASSERT_FALSE(schema["$ref"].get_string().get(ref));
    EXPECT_EQ(ref, "#/components/schemas/UserDto");
}

TEST(RequestBody, WithRequestBodyContentTypeOverridesMime)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Post, "/upload")
        .WithRequestSchemaJson("{\"type\":\"object\",\"properties\":{\"data\":{"
                               "\"type\":\"string\"}}}")
        .WithRequestBodyContentType("application/x-www-form-urlencoded")
        .WithRequestBodyRequired(true)
        .Handle([](baldr::HttpRequest&) -> std::string { return "ok"; });

    auto entries = router->Snapshot();
    ASSERT_EQ(entries.size(), 1u);

    auto ctIt = entries[0].options.metadata.find("requestBodyContentType");
    ASSERT_NE(ctIt, entries[0].options.metadata.end());
    EXPECT_EQ(ctIt->second, "application/x-www-form-urlencoded");

    auto reqIt = entries[0].options.metadata.find("requestBodyRequired");
    ASSERT_NE(reqIt, entries[0].options.metadata.end());
    EXPECT_EQ(reqIt->second, "true");

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
    simdjson::dom::object upload;
    ASSERT_FALSE(paths["/upload"].get_object().get(upload));
    simdjson::dom::object postOp;
    ASSERT_FALSE(upload["post"].get_object().get(postOp));
    simdjson::dom::object requestBody;
    ASSERT_FALSE(postOp["requestBody"].get_object().get(requestBody));

    bool requiredVal = false;
    ASSERT_FALSE(requestBody["required"].get_bool().get(requiredVal));
    EXPECT_TRUE(requiredVal);

    simdjson::dom::object content;
    ASSERT_FALSE(requestBody["content"].get_object().get(content));
    simdjson::dom::object form;
    ASSERT_FALSE(
        content["application/x-www-form-urlencoded"].get_object().get(form));
    simdjson::dom::object schema;
    ASSERT_FALSE(form["schema"].get_object().get(schema));
    std::string_view typeName;
    ASSERT_FALSE(schema["type"].get_string().get(typeName));
    EXPECT_EQ(typeName, "object");
}

TEST(RequestBody, RequestBodyOmittedWhenNoSchema)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/health")
        .Handle([](baldr::HttpRequest&) -> std::string { return "ok"; });

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
    simdjson::dom::object health;
    ASSERT_FALSE(paths["/health"].get_object().get(health));
    simdjson::dom::object getOp;
    ASSERT_FALSE(health["get"].get_object().get(getOp));
    simdjson::dom::element rbEl;
    EXPECT_EQ(getOp["requestBody"].get(rbEl), simdjson::NO_SUCH_FIELD);
}

TEST(RequestBody, WithRequestBodyRequiredDefaultsFalse)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Post, "/x")
        .WithRequestSchemaJson("{\"type\":\"object\"}")
        .Handle([](baldr::HttpRequest&) -> std::string { return "ok"; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("requestBodyRequired");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "false");
}

TEST(RequestBody, DefaultContentTypeIsApplicationJson)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Post, "/x")
        .WithRequestSchemaJson("{\"type\":\"object\"}")
        .Handle([](baldr::HttpRequest&) -> std::string { return "ok"; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].options.metadata.count("requestBodyContentType"), 0u);
}