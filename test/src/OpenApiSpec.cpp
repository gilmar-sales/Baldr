#include <Baldr/OpenApi/OpenApiSpecService.hpp>
#include <Baldr/OpenApi/RouteIntrospector.hpp>

#include <Baldr/Http/Router.hpp>

#include <gtest/gtest.h>

#include <simdjson.h>

#include <set>
#include <string>

class OpenApiSpec : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mRouter = skr::MakeArc<Router>();
    }

    skr::Arc<Router> mRouter;
};

TEST_F(OpenApiSpec, CachedIsStableUntilRegenerate)
{
    OpenApiOptions opts;
    opts.info.title = "Cached Test";

    OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(mRouter);
    const std::string& first = svc.Cached(mRouter);
    const std::string& second = svc.Cached(mRouter);
    EXPECT_EQ(&first, &second);
    EXPECT_FALSE(first.empty());
}

TEST_F(OpenApiSpec, SpecContainsPathTemplatesAndMethods)
{
    Baldr::RouteOptions getOpts;
    getOpts.summary     = "Get user";
    getOpts.operationId = "getUser";
    getOpts.tags        = { "users" };

    Baldr::RouteOptions postOpts;
    postOpts.summary     = "Create user";
    postOpts.operationId = "createUser";
    postOpts.tags        = { "users" };
    postOpts.deprecated  = true;

    mRouter->insert(
        HttpMethod::Get, "/users/:id", getOpts, "",
        [](HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(
        HttpMethod::Post, "/users", postOpts, "",
        [](HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>) {});

    OpenApiOptions opts;
    OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(mRouter);
    const std::string& spec = svc.Cached(mRouter);

    simdjson::dom::parser parser;
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

    std::string_view s;
    EXPECT_FALSE(getOp["summary"].get_string().get(s));
    EXPECT_EQ(s, "Get user");
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

TEST_F(OpenApiSpec, GroupPrefixIsPreservedInSnapshot)
{
    Baldr::RouteOptions opts;
    opts.tags = { "v1" };

    mRouter->insert(
        HttpMethod::Get, "/api/v1/orders/:id", opts, "/api/v1",
        [](HttpRequest&, HttpResponse&, skr::Arc<skr::ServiceProvider>) {});

    auto entries = mRouter->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].groupPrefix, "/api/v1");
    EXPECT_EQ(entries[0].pathTemplate, "/api/v1/orders/:id");
}
