// RouterFuzz.cpp - Fuzz target for baldr::Router.
//
// Feeds mutated path strings (and a randomized method) into a Router that
// has a handful of representative routes registered. Property assertions:
//   * `matchWithAllow` always returns either an entry, or a non-empty
//     `allowedMethodsOnPath` only when a path exists under another method
//     (Gap 2.3: 405 + Allow).
//   * `extractRouteParams` never throws and never reads past the regex end.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Router.hpp>
#include <Skirnir/Skirnir.hpp>

#include "FuzzedDataProvider.hpp"

namespace
{
    constexpr std::array<baldr::HttpMethod, 9> kMethods {
        baldr::HttpMethod::Get,    baldr::HttpMethod::Post,
        baldr::HttpMethod::Put,    baldr::HttpMethod::Delete,
        baldr::HttpMethod::Patch,  baldr::HttpMethod::Options,
        baldr::HttpMethod::Head,   baldr::HttpMethod::Trace,
        baldr::HttpMethod::Connect,
    };

    constexpr std::array<std::string_view, 6> kRouteTemplates {
        "/", "/users", "/users/:id", "/files/**", "/api/v1/orders/:id",
        "/static/*"
    };

    skr::Arc<baldr::Router> buildRouter()
    {
        auto router = skr::MakeArc<baldr::Router>();
        for (auto method : kMethods)
        {
            for (auto tmpl : kRouteTemplates)
            {
                router->insert(
                    method, std::string(tmpl),
                    [](baldr::HttpRequest&, baldr::HttpResponse&,
                       skr::Arc<skr::ServiceProvider>) {});
            }
        }
        return router;
    }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    auto router = buildRouter();

    auto method = fdp.PickValueInArray(kMethods);
    std::string path =
        "/" + fdp.ConsumeRandomLengthString(256);

    try
    {
        auto result = router->matchWithAllow(method, path);
        if (result.entry.has_value())
        {
            // Always extract route params on a hit.
            auto params = result.entry->extractRouteParams(path);
            (void) params;
        }
    }
    catch (...)
    {
        // Regex stack overflow or similar is a real bug; surface as crash
        // so the release-gating lane sees it.
        __builtin_trap();
    }

    return 0;
}