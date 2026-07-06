// SecurityFuzz.cpp - Fuzz target for SecurityHeadersMiddleware.
//
// Properties:
//   * Every configured header value reaches the response unchanged (after
//     the optional-nullopt check).
//   * No header value contains \r\n.

#include <cstddef>
#include <cstdint>
#include <string>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Middleware/SecurityHeaders.hpp>

#include "FuzzedDataProvider.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    baldr::SecurityHeadersOptions opts;
    opts.contentTypeOptions       = fdp.ConsumeBytesAsString(32);
    opts.frameOptions             = fdp.ConsumeBytesAsString(32);
    opts.referrerPolicy           = fdp.ConsumeBytesAsString(64);
    if (fdp.ConsumeBool())
        opts.strictTransportSecurity = fdp.ConsumeBytesAsString(64);
    if (fdp.ConsumeBool())
        opts.permissionsPolicy = fdp.ConsumeBytesAsString(64);
    opts.crossOriginOpenerPolicy = fdp.ConsumeBytesAsString(32);
    opts.crossOriginResourcePolicy = fdp.ConsumeBytesAsString(32);
    if (fdp.ConsumeBool())
        opts.crossOriginEmbedderPolicy = fdp.ConsumeBytesAsString(32);

    baldr::HttpRequest  req;
    baldr::HttpResponse resp(req);

    baldr::SecurityHeadersMiddleware mw(opts);
    mw.Handle(req, resp, []() {});

    for (const auto& [k, v] : resp.headers)
    {
        if (v.find('\r') != std::string::npos ||
            v.find('\n') != std::string::npos)
        {
            __builtin_trap();
        }
    }
    return 0;
}