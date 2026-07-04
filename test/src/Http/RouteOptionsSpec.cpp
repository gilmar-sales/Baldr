#include <Baldr/Application/WebApplication.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/Router.hpp>

#include <gtest/gtest.h>

class RouteOptionsSpec : public ::testing::Test
{
  protected:
    void SetUp() override { mRouter = skr::MakeArc<baldr::Router>(); }

    skr::Arc<baldr::Router> mRouter;
};

TEST_F(RouteOptionsSpec, LegacyTwoArgMapGetStillCompilesAndRegisters)
{
    skr::Arc<skr::ServiceProvider> sp;
    // WebApplication needs a root provider; use a real one to avoid
    // fighting the construction. We test the lower-level Router API
    // directly because constructing a full WebApplication here pulls in
    // the HTTP server.
    mRouter->insert(baldr::HttpMethod::Get, "/legacy",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto entries = mRouter->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_FALSE(entries[0].options.summary.has_value());
    EXPECT_TRUE(entries[0].options.tags.empty());
    EXPECT_FALSE(entries[0].options.deprecated);
}

TEST_F(RouteOptionsSpec, InsertWithOptionsCarriesAllFields)
{
    baldr::RouteOptions opts;
    opts.summary     = "List users";
    opts.description = "Returns a paginated list of users.";
    opts.tags        = { "users", "v1" };
    opts.operationId = "listUsers";
    opts.deprecated  = true;
    opts.consumes    = { "application/json" };
    opts.produces    = { "application/json" };
    opts.metadata    = { { "owner", "platform" } };

    mRouter->insert(baldr::HttpMethod::Get, "/users", opts, "",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto entries = mRouter->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    auto& e = entries[0];
    ASSERT_TRUE(e.options.summary.has_value());
    EXPECT_EQ(*e.options.summary, "List users");
    ASSERT_TRUE(e.options.description.has_value());
    EXPECT_EQ(*e.options.description, "Returns a paginated list of users.");
    EXPECT_EQ(e.options.tags, (std::vector<std::string> { "users", "v1" }));
    ASSERT_TRUE(e.options.operationId.has_value());
    EXPECT_EQ(*e.options.operationId, "listUsers");
    EXPECT_TRUE(e.options.deprecated);
    EXPECT_EQ(e.options.consumes,
              (std::vector<std::string> { "application/json" }));
    EXPECT_EQ(e.options.produces,
              (std::vector<std::string> { "application/json" }));
    EXPECT_EQ(e.options.metadata.at("owner"), "platform");
    EXPECT_EQ(e.pathTemplate, "/users");
    EXPECT_EQ(e.method, baldr::HttpMethod::Get);
}

TEST_F(RouteOptionsSpec, SnapshotWalksEveryRegisteredMethod)
{
    mRouter->insert(baldr::HttpMethod::Get, "/a",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(baldr::HttpMethod::Post, "/a",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
    mRouter->insert(baldr::HttpMethod::Delete, "/a",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto entries = mRouter->Snapshot();
    EXPECT_EQ(entries.size(), 3u);
    std::set<baldr::HttpMethod> seen;
    for (const auto& e : entries)
        seen.insert(e.method);
    EXPECT_TRUE(seen.contains(baldr::HttpMethod::Get));
    EXPECT_TRUE(seen.contains(baldr::HttpMethod::Post));
    EXPECT_TRUE(seen.contains(baldr::HttpMethod::Delete));
}

TEST_F(RouteOptionsSpec, PathTemplateTracksRoute)
{
    mRouter->insert(baldr::HttpMethod::Get, "/users/:id",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto entries = mRouter->Snapshot();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].pathTemplate, "/users/:id");
}

TEST_F(RouteOptionsSpec, MatchResultExposesRouteTemplate)
{
    mRouter->insert(baldr::HttpMethod::Get, "/users/:id",
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});

    auto result = mRouter->matchWithAllow(baldr::HttpMethod::Get, "/users/42");
    ASSERT_TRUE(result.entry.has_value());
    EXPECT_EQ(result.routeTemplate, "/users/:id");
    EXPECT_EQ(result.entry->pathTemplate, "/users/:id");
}
