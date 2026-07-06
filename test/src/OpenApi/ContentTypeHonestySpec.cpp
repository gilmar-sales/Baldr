#include <Baldr/Http/Response.hpp>
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

#include "../Http/UserDto.hpp"

namespace
{
    class SpecParser
    {
      public:
        explicit SpecParser(const std::string& spec)
        {
            EXPECT_FALSE(mParser.parse(spec).get(mDoc));
        }

        simdjson::dom::object responses()
        {
            simdjson::dom::object root;
            EXPECT_FALSE(mDoc.get_object().get(root));
            simdjson::dom::object paths;
            EXPECT_FALSE(root["paths"].get_object().get(paths));
            for (auto pathKv : paths)
            {
                simdjson::dom::object pathObj;
                if (pathKv.value.get_object().get(pathObj))
                    continue;
                for (auto opKv : pathObj)
                {
                    simdjson::dom::object opObj;
                    if (opKv.value.get_object().get(opObj))
                        continue;
                    simdjson::dom::object r;
                    if (!opObj["responses"].get_object().get(r))
                        return r;
                }
            }
            return simdjson::dom::object();
        }

      private:
        simdjson::dom::parser  mParser;
        simdjson::dom::element mDoc;
    };
} // namespace

TEST(ContentTypeHonesty, TextResultHandlerRendersTextPlain)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/text")
        .Handle([](baldr::HttpRequest&) -> baldr::TextResult {
            return baldr::TextResult("hi");
        });

    baldr::OpenApiOptions     opts;
    baldr::OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(router);
    SpecParser parser(svc.Cached(router));
    auto       responses = parser.responses();

    bool saw200 = false;
    for (auto kv : responses)
    {
        if (std::string_view(kv.key) != "200")
            continue;
        saw200 = true;
        simdjson::dom::object entry;
        ASSERT_FALSE(kv.value.get_object().get(entry));
        simdjson::dom::object content;
        if (entry["content"].get_object().get(content))
        {
            ADD_FAILURE() << "no content object for 200, entry="
                          << simdjson::minify(entry);
            continue;
        }
        simdjson::dom::object textPlain;
        EXPECT_FALSE(content["text/plain"].get_object().get(textPlain));
    }
    EXPECT_TRUE(saw200);
}

TEST(ContentTypeHonesty, NoContentResultHandlerRendersNoContent)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Delete, "/x")
        .WithResponseType<UserDto>()
        .Handle([](baldr::HttpRequest&)
                    -> std::variant<UserDto, baldr::NoContentResult> {
            return baldr::NoContentResult();
        });

    baldr::OpenApiOptions     opts;
    baldr::OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(router);
    SpecParser parser(svc.Cached(router));
    auto       responses = parser.responses();

    bool saw204 = false;
    for (auto kv : responses)
    {
        if (std::string_view(kv.key) != "204")
            continue;
        saw204 = true;
        simdjson::dom::object entry;
        ASSERT_FALSE(kv.value.get_object().get(entry));
        simdjson::dom::object content;
        ASSERT_FALSE(entry["content"].get_object().get(content));
        EXPECT_EQ(content.size(), 0u);
    }
    EXPECT_TRUE(saw204);
}

TEST(ContentTypeHonesty, WithResponseContentTypeOverridesDefault)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/raw")
        .WithResponseSchemaJson("{\"type\":\"string\",\"format\":\"binary\"}")
        .WithResponseContentType("application/octet-stream")
        .Handle([](baldr::HttpRequest&) -> baldr::OkResult {
            return baldr::OkResult("data");
        });

    baldr::OpenApiOptions     opts;
    baldr::OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(router);
    SpecParser parser(svc.Cached(router));
    auto       responses = parser.responses();

    bool saw200 = false;
    for (auto kv : responses)
    {
        if (std::string_view(kv.key) != "200")
            continue;
        saw200 = true;
        simdjson::dom::object entry;
        ASSERT_FALSE(kv.value.get_object().get(entry));
        simdjson::dom::object content;
        ASSERT_FALSE(entry["content"].get_object().get(content));
        simdjson::dom::object octet;
        EXPECT_FALSE(
            content["application/octet-stream"].get_object().get(octet));
    }
    EXPECT_TRUE(saw200);
}

TEST(ContentTypeHonesty, TextResultVariantWithTypedResultRendersBothTypes)
{
    skr::Arc<baldr::Router> router = skr::MakeArc<baldr::Router>();
    baldr::RouteRegistration(*router, baldr::HttpMethod::Get, "/mix")
        .Handle([](baldr::HttpRequest&)
                    -> std::variant<baldr::TextResult, baldr::NotFoundResult> {
            return baldr::TextResult("hi");
        });

    baldr::OpenApiOptions     opts;
    baldr::OpenApiSpecService svc(std::move(opts));
    svc.Regenerate(router);
    SpecParser parser(svc.Cached(router));
    auto       responses = parser.responses();

    bool saw200 = false;
    bool saw404 = false;
    for (auto kv : responses)
    {
        std::string_view      k = kv.key;
        simdjson::dom::object entry;
        ASSERT_FALSE(kv.value.get_object().get(entry));
        simdjson::dom::object content;
        ASSERT_FALSE(entry["content"].get_object().get(content));
        simdjson::dom::object textPlain;
        const bool            hasTextPlain =
            !content["text/plain"].get_object().get(textPlain);
        if (k == "200")
        {
            saw200 = true;
            EXPECT_TRUE(hasTextPlain);
        }
        else if (k == "404")
        {
            saw404 = true;
            EXPECT_TRUE(hasTextPlain);
        }
    }
    EXPECT_TRUE(saw200);
    EXPECT_TRUE(saw404);
}
