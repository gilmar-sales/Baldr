#include <Baldr/OpenApi/JsonSchemaEmitter.hpp>
#include <Baldr/OpenApi/RouteIntrospector.hpp>
#include <Baldr/OpenApi/SpecBuilder.hpp>
#include <Baldr/OpenApi/OpenApiOptions.hpp>

#include <Baldr/Http/Router.hpp>
#include <Baldr/Http/RouteRegistration.hpp>

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
    SchemaRegistry reg;
    std::string schema = EmitAndRegister<ReflectableDevice>(
        reg);

    simdjson::dom::parser parser;
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
    SchemaRegistry reg;
    EmitAndRegister<ReflectableDevice>(reg);
    EmitAndRegister<ReflectableDevice>(reg);

    EXPECT_EQ(reg.Schemas().size(), 1u);
    EXPECT_TRUE(reg.Contains("ReflectableDevice"));
}

TEST(JsonSchemaEmitterSpec, TranslatePathReplacesColonParam)
{
    EXPECT_EQ(TranslatePath("/users/:id"),
              "/users/{id}");
    EXPECT_EQ(TranslatePath("/a/:b/c"),
              "/a/{b}/c");
    EXPECT_EQ(TranslatePath("/files/**"),
              "/files/{filepath}");
    EXPECT_EQ(TranslatePath("/"), "/");
}

TEST(SpecBuilderSpec, RendersOpenApiThreeDocument)
{
    OpenApiOptions opts;
    opts.info.title       = "Test";
    opts.info.version     = "1.0.0";
    opts.info.description = "Spec test";

    std::vector<RouteEntry> entries;

    RouteEntry get;
    get.pathTemplate = "/users/:id";
    get.method       = HttpMethod::Get;
    get.options.summary     = "Fetch user";
    get.options.tags        = { "users" };
    get.options.operationId = "getUser";
    get.options.metadata["responseSchemaJson"] =
        "{\"type\":\"object\"}";
    entries.push_back(get);

    RouteEntry post;
    post.pathTemplate = "/users";
    post.method       = HttpMethod::Post;
    post.options.summary     = "Create user";
    post.options.tags        = { "users" };
    post.options.operationId = "createUser";
    post.options.deprecated  = true;
    entries.push_back(post);

    SpecBuilder builder(opts);
    std::string spec = builder.Render(entries);

    simdjson::dom::parser parser;
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
    RouteEntry mount;
    mount.pathTemplate = "/openapi.json";
    mount.method       = HttpMethod::Get;
    std::vector<RouteEntry> entries { mount };

    OpenApiOptions opts;
    SpecBuilder  builder(opts);
    std::string                  spec = builder.Render(entries);

    EXPECT_NE(spec.find("/openapi.json"), std::string::npos);
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
    int    id;
    void*  raw;
};

static_assert(IsReflectableStruct<ReflectableDevice>,
              "ReflectableDevice must be auto-derivable");
static_assert(IsReflectableStruct<AutoDevice>,
              "AutoDevice must be auto-derivable");
static_assert(!IsReflectableStruct<UnsupportedDevice>,
              "UnsupportedDevice has a void* field, must NOT be derivable");
static_assert(!IsReflectableStruct<std::string>,
              "std::string is not a struct-shaped reflectable type");
static_assert(!IsReflectableStruct<int>,
              "int is not a struct-shaped reflectable type");
static_assert(
    !IsReflectableStruct<std::vector<int>>,
    "std::vector is not a reflectable struct (no reflection members)");

TEST(JsonSchemaEmitterSpec, IsReflectableStructMatchesExpectations)
{
    EXPECT_TRUE(IsReflectableStruct<ReflectableDevice>);
    EXPECT_TRUE(IsReflectableStruct<AutoDevice>);
    EXPECT_FALSE(IsReflectableStruct<UnsupportedDevice>);
    EXPECT_FALSE(IsReflectableStruct<std::string>);
    EXPECT_FALSE(IsReflectableStruct<int>);
    EXPECT_FALSE(IsReflectableStruct<std::vector<int>>);
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForReturnsRefFragment)
{
    SchemaRegistry reg;
    auto ref = TryEmitRefFor<AutoDevice>(reg);

    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(*ref,
              "{\"$ref\":\"#/components/schemas/AutoDevice\"}");
    EXPECT_TRUE(reg.Contains("AutoDevice"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForDeduplicatesAcrossCalls)
{
    SchemaRegistry reg;
    TryEmitRefFor<AutoDevice>(reg);
    TryEmitRefFor<AutoDevice>(reg);

    EXPECT_EQ(reg.Schemas().size(), 1u);
    EXPECT_TRUE(reg.Contains("AutoDevice"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForUnsupporedTypeYieldsNullopt)
{
    SchemaRegistry reg;
    auto                           ref =
        TryEmitRefFor<int>(reg);
    EXPECT_FALSE(ref.has_value());
    EXPECT_FALSE(reg.Contains("int"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForVectorOfReflectableEmitsArraySchema)
{
    SchemaRegistry reg;
    auto ref = TryEmitRefFor<std::vector<AutoDevice>>(reg);

    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(
        *ref,
        "{\"type\":\"array\","
        "\"items\":{\"$ref\":\"#/components/schemas/AutoDevice\"}}");
    EXPECT_TRUE(reg.Contains("AutoDevice"));
}

TEST(JsonSchemaEmitterSpec, TryEmitRefForVectorOfUnsupportedYieldsNullopt)
{
    SchemaRegistry reg;
    auto ref = TryEmitRefFor<std::vector<int>>(reg);

    EXPECT_FALSE(ref.has_value());
    EXPECT_FALSE(reg.Contains("int"));
}

TEST(RouteRegistrationAutoSchema, AutoDerivesReflectableReturnType)
{
    Router router;
    Baldr::RouteRegistration(router, HttpMethod::Get, "/auto")
        .WithSummary("auto")
        .Handle([](HttpRequest&) -> AutoDevice {
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
    Router router;
    Baldr::RouteRegistration(router, HttpMethod::Get, "/explicit")
        .WithResponseSchemaJson("{\"type\":\"object\"}")
        .Handle([](HttpRequest&) -> AutoDevice {
            return AutoDevice { 1, "u", 3.3, true };
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("responseSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second, "{\"type\":\"object\"}");
}

TEST(RouteRegistrationAutoSchema,
     UnsupporedReturnTypeLeavesMetadataEmpty)
{
    Router router;
    Baldr::RouteRegistration(router, HttpMethod::Get, "/raw")
        .Handle([](HttpRequest&) -> int { return 42; });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].options.metadata.count("responseSchemaJson"), 0u);
}

TEST(RouteRegistrationAutoSchema, WithRequestTypeDerivesRequestSchema)
{
    Router router;
    Baldr::RouteRegistration(router, HttpMethod::Post, "/echo")
        .WithRequestType<AutoDevice>()
        .Handle([](HttpRequest&) -> std::string {
            return "ok";
        });

    auto entries = router.Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto it = entries[0].options.metadata.find("requestSchemaJson");
    ASSERT_NE(it, entries[0].options.metadata.end());
    EXPECT_EQ(it->second,
              "{\"$ref\":\"#/components/schemas/AutoDevice\"}");
    EXPECT_TRUE(router.SchemaRegistrySlot()->Contains("AutoDevice"));
}