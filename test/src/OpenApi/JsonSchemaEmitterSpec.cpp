#include <Baldr/OpenApi/JsonSchemaEmitter.hpp>
#include <Baldr/OpenApi/OpenApiOptions.hpp>
#include <Baldr/OpenApi/RouteIntrospector.hpp>
#include <Baldr/OpenApi/SpecBuilder.hpp>

#include <Baldr/Http/RouteRegistration.hpp>
#include <Baldr/Http/Router.hpp>

#include <gtest/gtest.h>

#include <simdjson.h>

#include <string>

struct ReflectableDevice
{
    int         id;
    std::string uuid;
    double      voltage;
    bool        active;
};

TEST(JsonSchemaEmitterSpec, EmitsStructSchemaWithPrimitiveFields)
{
    baldr::SchemaRegistry reg;
    std::string schema = baldr::EmitAndRegister<ReflectableDevice>(reg);

    simdjson::dom::parser  parser;
    simdjson::dom::element doc;
    ASSERT_FALSE(parser.parse(schema).get(doc));

    simdjson::dom::object obj;
    ASSERT_FALSE(doc.get_object().get(obj));

    std::string_view s;
    EXPECT_FALSE(obj["$schema"].get_string().get(s));
    EXPECT_EQ(s, "http://json-schema.org/draft-07/schema#");
    EXPECT_FALSE(obj["type"].get_string().get(s));
    EXPECT_EQ(s, "object");

    simdjson::dom::object props;
    ASSERT_FALSE(obj["properties"].get_object().get(props));

    simdjson::dom::element idEl;
    ASSERT_FALSE(props["id"].get(idEl));
    EXPECT_FALSE(idEl.get_object().value()["type"].get_string().get(s));
    EXPECT_EQ(s, "integer");

    simdjson::dom::element uuidEl;
    ASSERT_FALSE(props["uuid"].get(uuidEl));
    EXPECT_FALSE(uuidEl.get_object().value()["type"].get_string().get(s));
    EXPECT_EQ(s, "string");

    simdjson::dom::element voltageEl;
    ASSERT_FALSE(props["voltage"].get(voltageEl));
    EXPECT_FALSE(voltageEl.get_object().value()["type"].get_string().get(s));
    EXPECT_EQ(s, "number");

    simdjson::dom::element activeEl;
    ASSERT_FALSE(props["active"].get(activeEl));
    EXPECT_FALSE(activeEl.get_object().value()["type"].get_string().get(s));
    EXPECT_EQ(s, "boolean");

    simdjson::dom::array required;
    ASSERT_FALSE(obj["required"].get_array().get(required));
    EXPECT_EQ(required.size(), 4u);
}

TEST(JsonSchemaEmitterSpec, RegistryDeduplicatesByTypeName)
{
    baldr::SchemaRegistry reg;
    baldr::EmitAndRegister<ReflectableDevice>(reg);
    baldr::EmitAndRegister<ReflectableDevice>(reg);

    EXPECT_EQ(reg.Schemas().size(), 1u);
    EXPECT_TRUE(reg.Contains("ReflectableDevice"));
}

TEST(JsonSchemaEmitterSpec, TranslatePathReplacesColonParam)
{
    EXPECT_EQ(baldr::TranslatePath("/users/:id"), "/users/{id}");
    EXPECT_EQ(baldr::TranslatePath("/a/:b/c"), "/a/{b}/c");
    EXPECT_EQ(baldr::TranslatePath("/files/**"), "/files/{filepath}");
    EXPECT_EQ(baldr::TranslatePath("/"), "/");
}

TEST(SpecBuilderSpec, RendersOpenApiThreeDocument)
{
    baldr::OpenApiOptions opts;
    opts.info.title       = "Test";
    opts.info.version     = "1.0.0";
    opts.info.description = "Spec test";

    std::vector<baldr::RouteEntry> entries;

    baldr::RouteEntry get;
    get.pathTemplate                           = "/users/:id";
    get.method                                 = baldr::HttpMethod::Get;
    get.options.summary                        = "Fetch user";
    get.options.tags                           = { "users" };
    get.options.operationId                    = "getUser";
    get.options.metadata["responseSchemaJson"] = "{\"type\":\"object\"}";
    entries.push_back(get);

    baldr::RouteEntry post;
    post.pathTemplate        = "/users";
    post.method              = baldr::HttpMethod::Post;
    post.options.summary     = "Create user";
    post.options.tags        = { "users" };
    post.options.operationId = "createUser";
    post.options.deprecated  = true;
    entries.push_back(post);

    baldr::SpecBuilder builder(opts);
    std::string        spec = builder.Render(entries);

    simdjson::dom::parser  parser;
    simdjson::dom::element doc;
    ASSERT_FALSE(parser.parse(spec).get(doc));

    simdjson::dom::object root;
    ASSERT_FALSE(doc.get_object().get(root));

    std::string_view s;
    EXPECT_FALSE(root["openapi"].get_string().get(s));
    EXPECT_EQ(s, "3.0.3");

    simdjson::dom::object info;
    ASSERT_FALSE(root["info"].get_object().get(info));
    EXPECT_FALSE(info["title"].get_string().get(s));
    EXPECT_EQ(s, "Test");

    simdjson::dom::object paths;
    ASSERT_FALSE(root["paths"].get_object().get(paths));

    simdjson::dom::object usersById;
    ASSERT_FALSE(paths["/users/{id}"].get_object().get(usersById));
    simdjson::dom::object getOp;
    ASSERT_FALSE(usersById["get"].get_object().get(getOp));
    EXPECT_FALSE(getOp["summary"].get_string().get(s));
    EXPECT_EQ(s, "Fetch user");
    EXPECT_FALSE(getOp["operationId"].get_string().get(s));
    EXPECT_EQ(s, "getUser");

    simdjson::dom::object users;
    ASSERT_FALSE(paths["/users"].get_object().get(users));
    simdjson::dom::object postOp;
    ASSERT_FALSE(users["post"].get_object().get(postOp));
    bool deprecated = false;
    EXPECT_FALSE(postOp["deprecated"].get_bool().get(deprecated));
    EXPECT_TRUE(deprecated);
}

TEST(SpecBuilderSpec, MountedOpenApiPathIsExcludedFromSpec)
{
    // The extension registers /openapi.json via the same Router, so the
    // spec would otherwise include its own endpoint. The extension is
    // responsible for filtering; SpecBuilder itself includes every entry.
    // This test pins down the current behaviour so any future change is
    // intentional.
    baldr::RouteEntry mount;
    mount.pathTemplate = "/openapi.json";
    mount.method       = baldr::HttpMethod::Get;
    std::vector<baldr::RouteEntry> entries { mount };

    baldr::OpenApiOptions opts;
    baldr::SpecBuilder    builder(opts);
    std::string           spec = builder.Render(entries);

    EXPECT_NE(spec.find("/openapi.json"), std::string::npos);
}

TEST(SpecBuilderSpec, EmitsQueryAndPathParametersFromMetadata)
{
    baldr::RouteEntry entry;
    entry.pathTemplate = "/users/:id";
    entry.method       = baldr::HttpMethod::Get;
    entry.options.metadata["queryParametersJson"] =
        "[{\"name\":\"name\",\"in\":\"query\",\"required\":true,"
        "\"schema\":{\"type\":\"string\"}}]";
    entry.options.metadata["pathParametersJson"] =
        "[{\"name\":\"id\",\"in\":\"path\",\"required\":true,"
        "\"schema\":{\"type\":\"string\"}}]";
    std::vector<baldr::RouteEntry> entries { entry };

    baldr::OpenApiOptions opts;
    baldr::SpecBuilder    builder(opts);
    std::string           spec = builder.Render(entries);

    simdjson::dom::parser  parser;
    simdjson::dom::element doc;
    ASSERT_FALSE(parser.parse(spec).get(doc));

    simdjson::dom::object root;
    ASSERT_FALSE(doc.get_object().get(root));

    simdjson::dom::object paths;
    ASSERT_FALSE(root["paths"].get_object().get(paths));
    simdjson::dom::object users;
    ASSERT_FALSE(paths["/users/{id}"].get_object().get(users));
    simdjson::dom::object getOp;
    ASSERT_FALSE(users["get"].get_object().get(getOp));

    simdjson::dom::array params;
    ASSERT_FALSE(getOp["parameters"].get_array().get(params));
    EXPECT_EQ(params.size(), 2u);
}

struct AutoDevice
{
    int         id;
    std::string uuid;
    double      voltage;
    bool        active;
};

struct UnsupportedDevice
{
    int   id;
    void* raw;
};

static_assert(baldr::IsReflectableStruct<ReflectableDevice>,
              "ReflectableDevice must be auto-derivable");
static_assert(baldr::IsReflectableStruct<AutoDevice>,
              "AutoDevice must be auto-derivable");
static_assert(!baldr::IsReflectableStruct<UnsupportedDevice>,
              "UnsupportedDevice has a void* field, must NOT be derivable");
static_assert(!baldr::IsReflectableStruct<std::string>,
              "std::string is not a struct-shaped reflectable type");
static_assert(!baldr::IsReflectableStruct<int>,
              "int is not a struct-shaped reflectable type");
static_assert(
    !baldr::IsReflectableStruct<std::vector<int>>,
    "std::vector is not a reflectable struct (no reflection members)");

TEST(JsonSchemaEmitterSpec, IsReflectableStructMatchesExpectations)
{
    EXPECT_TRUE(baldr::IsReflectableStruct<ReflectableDevice>);
    EXPECT_TRUE(baldr::IsReflectableStruct<AutoDevice>);
    EXPECT_FALSE(baldr::IsReflectableStruct<UnsupportedDevice>);
    EXPECT_FALSE(baldr::IsReflectableStruct<std::string>);
    EXPECT_FALSE(baldr::IsReflectableStruct<int>);
    EXPECT_FALSE(baldr::IsReflectableStruct<std::vector<int>>);
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForReturnsRefFragment)
{
    baldr::SchemaRegistry reg;
    auto                  ref = baldr::TryEmitRefFor<AutoDevice>(reg);

    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(*ref, "{\"$ref\":\"#/components/schemas/AutoDevice\"}");
    EXPECT_TRUE(reg.Contains("AutoDevice"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForDeduplicatesAcrossCalls)
{
    baldr::SchemaRegistry reg;
    baldr::TryEmitRefFor<AutoDevice>(reg);
    baldr::TryEmitRefFor<AutoDevice>(reg);

    EXPECT_EQ(reg.Schemas().size(), 1u);
    EXPECT_TRUE(reg.Contains("AutoDevice"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForUnsupporedTypeYieldsNullopt)
{
    baldr::SchemaRegistry reg;
    auto                  ref = baldr::TryEmitRefFor<int>(reg);
    EXPECT_FALSE(ref.has_value());
    EXPECT_FALSE(reg.Contains("int"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForVectorOfReflectableEmitsArraySchema)
{
    baldr::SchemaRegistry reg;
    auto ref = baldr::TryEmitRefFor<std::vector<AutoDevice>>(reg);

    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(*ref,
              "{\"type\":\"array\","
              "\"items\":{\"$ref\":\"#/components/schemas/AutoDevice\"}}");
    EXPECT_TRUE(reg.Contains("AutoDevice"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForVectorOfUnsupportedYieldsNullopt)
{
    baldr::SchemaRegistry reg;
    auto                  ref = baldr::TryEmitRefFor<std::vector<int>>(reg);

    EXPECT_FALSE(ref.has_value());
    EXPECT_FALSE(reg.Contains("int"));
}

TEST(RouteRegistrationAutoSchema, AutoDerivesReflectableReturnType)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/auto")
        .WithSummary("auto")
        .Handle([](baldr::HttpRequest&) -> AutoDevice {
            return AutoDevice { 1, "u", 3.3, true };
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"$ref\":\"#/components/schemas/AutoDevice\"}");

    ASSERT_NE(router.SchemaRegistrySlot(), nullptr);
    EXPECT_TRUE(router.SchemaRegistrySlot()->Contains("AutoDevice"));
}

TEST(RouteRegistrationAutoSchema, ExplicitSchemaWinsOverAutoDerivation)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/explicit")
        .WithResponseSchemaJson("{\"type\":\"object\"}")
        .Handle([](baldr::HttpRequest&) -> AutoDevice {
            return AutoDevice { 1, "u", 3.3, true };
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"type\":\"object\"}");
}

TEST(RouteRegistrationAutoSchema, UnsupporedReturnTypeLeavesMetadataEmpty)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Get, "/raw")
        .Handle([](baldr::HttpRequest&) -> int { return 42; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].options.metadata.count("responseSchemaJson"), 0u);
}

TEST(RouteRegistrationAutoSchema, WithRequestTypeDerivesRequestSchema)
{
    baldr::Router router;
    baldr::RouteRegistration(router, baldr::HttpMethod::Post, "/echo")
        .WithRequestType<AutoDevice>()
        .Handle([](baldr::HttpRequest&) -> std::string { return "ok"; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("requestSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"$ref\":\"#/components/schemas/AutoDevice\"}");
    EXPECT_TRUE(router.SchemaRegistrySlot()->Contains("AutoDevice"));
}