#include <Baldr/OpenApi/MapScalarUi.hpp>

#include <Baldr/BaldrExtension.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Router.hpp>
#include <Baldr/Http/ServerOptions.hpp>

#include <Skirnir/Common/Arc.hpp>
#include <Skirnir/DependencyInjection/ApplicationBuilder.hpp>
#include <Skirnir/DependencyInjection/ServiceProvider.hpp>
#include <Skirnir/Logging/LogLevel.hpp>
#include <Skirnir/Logging/LogRecord.hpp>
#include <Skirnir/Logging/LogSinks/ILogSink.hpp>
#include <Skirnir/Logging/Logger.hpp>

#include <gtest/gtest.h>

#include <mutex>
#include <string>
#include <vector>

namespace
{

    class CapturingSink final : public skr::ILogSink
    {
      public:
        void Write(const skr::LogRecord& record) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mRecords.push_back(record);
        }

        const std::vector<skr::LogRecord>& Records() const { return mRecords; }

      private:
        mutable std::mutex          mMutex;
        std::vector<skr::LogRecord> mRecords;
    };

    skr::Arc<baldr::WebApplication> buildApp(skr::Arc<CapturingSink> sinkArc,
                                             short port = 8080)
    {
        auto builder         = skr::ApplicationBuilder();
        auto logger          = skr::MakeArc<skr::LoggerOptions>();
        logger->asyncEnabled = false;
        logger->AddSink(sinkArc);
        builder.GetServiceCollection()->AddSingleton<skr::LoggerOptions>(
            logger);

        builder.WithExtension<baldr::BaldrExtension>(
            [&port](baldr::BaldrExtension&) {});

        baldr::HttpServerOptions opts;
        opts.port = port;
        builder.GetServiceCollection()->AddSingleton<baldr::HttpServerOptions>(
            skr::MakeArc<baldr::HttpServerOptions>(opts));

        return builder.Build<baldr::WebApplication>();
    }

    baldr::RouteEntry matchRoute(const skr::Arc<baldr::Router>& router,
                                 const std::string&             path)
    {
        auto entry = router->match(baldr::HttpMethod::Get, path);
        EXPECT_TRUE(entry.has_value()) << "no route registered for " << path;
        return std::move(*entry);
    }

    void invoke(const baldr::RouteEntry&              entry,
                const std::string&                    path,
                const skr::Arc<skr::ServiceProvider>& sp,
                baldr::HttpResponse&                  out)
    {
        baldr::HttpRequest req;
        req.method = baldr::HttpMethod::Get;
        req.path   = path;
        (void) entry.extractRouteParams(path);
        entry.handler(req, out, sp);
    }

    std::string findOrNpos(const std::string& haystack,
                           const std::string& needle)
    {
        auto p = haystack.find(needle);
        return p == std::string::npos ? std::string {} : std::string(needle);
    }

} // namespace

TEST(MapScalarUiSpec, RegistersThreeRoutesAtDefaults)
{
    auto app = buildApp(skr::MakeArc<CapturingSink>(), 8080);
    baldr::MapScalarUi(*app);

    auto router = app->GetRouter();
    EXPECT_TRUE(router->match(baldr::HttpMethod::Get, "/scalar").has_value());
    EXPECT_TRUE(
        router->match(baldr::HttpMethod::Get, "/scalar/scalar-reference.js")
            .has_value());
    EXPECT_TRUE(router->match(baldr::HttpMethod::Get, "/scalar/styles.css")
                    .has_value());
    EXPECT_FALSE(
        router->match(baldr::HttpMethod::Get, "/other/scalar-reference.js")
            .has_value());
}

TEST(MapScalarUiSpec, NormalisesMissingLeadingSlash)
{
    auto app = buildApp(skr::MakeArc<CapturingSink>(), 8080);
    baldr::MapScalarUi(*app, "docs");

    auto router = app->GetRouter();
    EXPECT_TRUE(router->match(baldr::HttpMethod::Get, "/docs").has_value());
    EXPECT_TRUE(
        router->match(baldr::HttpMethod::Get, "/docs/scalar-reference.js")
            .has_value());
    EXPECT_TRUE(
        router->match(baldr::HttpMethod::Get, "/docs/styles.css").has_value());
}

TEST(MapScalarUiSpec, JsAssetReturnsEmbeddedBytesWithJsContentType)
{
    auto app = buildApp(skr::MakeArc<CapturingSink>(), 8080);
    baldr::MapScalarUi(*app);

    auto router = app->GetRouter();
    auto entry  = matchRoute(router, "/scalar/scalar-reference.js");

    baldr::HttpResponse                  resp;
    const skr::Arc<skr::ServiceProvider> sp = app->GetRootServiceProvider();
    invoke(entry, "/scalar/scalar-reference.js", sp, resp);

    EXPECT_EQ(resp.statusCode, baldr::StatusCode::OK);
    EXPECT_EQ(resp.headers.at("Content-Type"),
              "application/javascript; charset=utf-8");
    EXPECT_FALSE(resp.body.empty());
}

TEST(MapScalarUiSpec, CssAssetReturnsEmbeddedBytesWithCssContentType)
{
    auto app = buildApp(skr::MakeArc<CapturingSink>(), 8080);
    baldr::MapScalarUi(*app);

    auto router = app->GetRouter();
    auto entry  = matchRoute(router, "/scalar/styles.css");

    baldr::HttpResponse                  resp;
    const skr::Arc<skr::ServiceProvider> sp = app->GetRootServiceProvider();
    invoke(entry, "/scalar/styles.css", sp, resp);

    EXPECT_EQ(resp.statusCode, baldr::StatusCode::OK);
    EXPECT_EQ(resp.headers.at("Content-Type"), "text/css; charset=utf-8");
    EXPECT_FALSE(resp.body.empty());
}

TEST(MapScalarUiSpec, HtmlReplacesAllPlaceholders)
{
    auto app = buildApp(skr::MakeArc<CapturingSink>(), 8080);
    baldr::MapScalarUi(*app, "/scalar", "/openapi.json", "Devices API");

    auto router = app->GetRouter();
    auto entry  = matchRoute(router, "/scalar");

    baldr::HttpResponse                  resp;
    const skr::Arc<skr::ServiceProvider> sp = app->GetRootServiceProvider();
    invoke(entry, "/scalar", sp, resp);

    EXPECT_EQ(resp.statusCode, baldr::StatusCode::OK);
    EXPECT_EQ(resp.headers.at("Content-Type"), "text/html; charset=utf-8");

    EXPECT_NE(resp.body.find("Devices API"), std::string::npos);
    EXPECT_NE(resp.body.find("/scalar/scalar-reference.js"), std::string::npos);
    EXPECT_NE(resp.body.find("/scalar/styles.css"), std::string::npos);
    EXPECT_NE(resp.body.find("/openapi.json"), std::string::npos);

    EXPECT_EQ(resp.body.find("__TITLE__"), std::string::npos);
    EXPECT_EQ(resp.body.find("__JS_URL__"), std::string::npos);
    EXPECT_EQ(resp.body.find("__STYLES_URL__"), std::string::npos);
    EXPECT_EQ(resp.body.find("__SPEC_URL__"), std::string::npos);
    EXPECT_EQ(resp.body.find("__CONFIGURATION__"), std::string::npos);
    EXPECT_NE(resp.body.find("{}"), std::string::npos);
}

TEST(MapScalarUiSpec, HtmlWithCustomMountAndTitleReplacesPlaceholders)
{
    auto app = buildApp(skr::MakeArc<CapturingSink>(), 8080);
    baldr::MapScalarUi(*app, "api-ref", "openapi/v1.json", "My Title");

    auto router = app->GetRouter();
    auto entry  = matchRoute(router, "/api-ref");

    baldr::HttpResponse                  resp;
    const skr::Arc<skr::ServiceProvider> sp = app->GetRootServiceProvider();
    invoke(entry, "/api-ref", sp, resp);

    EXPECT_EQ(resp.headers.at("Content-Type"), "text/html; charset=utf-8");
    EXPECT_NE(resp.body.find("My Title"), std::string::npos);
    EXPECT_NE(resp.body.find("/api-ref/scalar-reference.js"),
              std::string::npos);
    EXPECT_NE(resp.body.find("/api-ref/styles.css"), std::string::npos);
    EXPECT_NE(resp.body.find("openapi/v1.json"), std::string::npos);
}

TEST(MapScalarUiSpec, LogsInformationWhenHttpServerOptionsRegistered)
{
    auto arcSink = skr::MakeArc<CapturingSink>();
    auto app     = buildApp(arcSink, 4242);

    baldr::MapScalarUi(*app, "/scalar", "/openapi.json", "API");

    const auto& records = arcSink->Records();
    ASSERT_FALSE(records.empty());
    const auto& rec = records.back();
    for (const auto& r : records)
    {
        EXPECT_EQ(r.level, skr::LogLevel::Information) << r.message;
    }
    EXPECT_NE(rec.message.find("Scalar UI listening at http://0.0.0.0:4242"),
              std::string::npos)
        << rec.message;
    EXPECT_NE(rec.message.find("/scalar"), std::string::npos) << rec.message;
    EXPECT_NE(rec.message.find("/openapi.json"), std::string::npos)
        << rec.message;
}
