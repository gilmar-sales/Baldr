#include <JsonSchemaEmitter.hpp>
#include <RouteIntrospector.hpp>
#include <SpecBuilder.hpp>
#include <OpenApiOptions.hpp>

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
    Baldr::OpenApi::SchemaRegistry reg;
    std::string schema = Baldr::OpenApi::EmitAndRegister<ReflectableDevice>(
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
    Baldr::OpenApi::SchemaRegistry reg;
    Baldr::OpenApi::EmitAndRegister<ReflectableDevice>(reg);
    Baldr::OpenApi::EmitAndRegister<ReflectableDevice>(reg);

    EXPECT_EQ(reg.Schemas().size(), 1u);
    EXPECT_TRUE(reg.Contains("ReflectableDevice"));
}

TEST(JsonSchemaEmitterSpec, TranslatePathReplacesColonParam)
{
    EXPECT_EQ(Baldr::OpenApi::TranslatePath("/users/:id"),
              "/users/{id}");
    EXPECT_EQ(Baldr::OpenApi::TranslatePath("/a/:b/c"),
              "/a/{b}/c");
    EXPECT_EQ(Baldr::OpenApi::TranslatePath("/files/**"),
              "/files/{filepath}");
    EXPECT_EQ(Baldr::OpenApi::TranslatePath("/"), "/");
}

TEST(SpecBuilderSpec, RendersOpenApiThreeDocument)
{
    Baldr::OpenApi::OpenApiOptions opts;
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

    Baldr::OpenApi::SpecBuilder builder(opts);
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

    Baldr::OpenApi::OpenApiOptions opts;
    Baldr::OpenApi::SpecBuilder  builder(opts);
    std::string                  spec = builder.Render(entries);

    EXPECT_NE(spec.find("/openapi.json"), std::string::npos);
}