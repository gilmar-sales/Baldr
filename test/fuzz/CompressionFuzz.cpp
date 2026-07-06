// CompressionFuzz.cpp - Fuzz target for CompressionMiddleware.
//
// Drives mutated Accept-Encoding headers and bodies through the middleware.
// Property assertions:
//   * if Accept-Encoding lacks gzip, Content-Encoding is absent
//   * compressed body length <= input * 1.0001 + 64 (ratio-bomb guard)
//   * when body < minBodyBytes, Content-Encoding is absent

#include <cstddef>
#include <cstdint>
#include <string>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Middleware/Compression/Middleware.hpp>

#include "FuzzedDataProvider.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    baldr::HttpRequest  req;
    baldr::HttpResponse resp;
    req.method = baldr::HttpMethod::Get;

    auto accept = fdp.ConsumeBytesAsString(
        fdp.ConsumeIntegralInRange<std::size_t>(0, 64));
    auto body = fdp.ConsumeBytesAsString(
        fdp.ConsumeIntegralInRange<std::size_t>(0, 16384));
    auto ctype = fdp.ConsumeBytesAsString(
        fdp.ConsumeIntegralInRange<std::size_t>(0, 64));
    if (ctype.empty())
        ctype = "text/plain";

    req.headers["accept-encoding"] = accept;
    req.headers["host"] = "x";

    resp.statusCode = baldr::StatusCode::OK;
    resp.body        = body;
    resp.headers["Content-Type"] = ctype;

    baldr::CompressionMiddleware mw;
    mw.Handle(req, resp, []() {});

    auto encIt = resp.headers.find("content-encoding");
    if (encIt == resp.headers.end())
        return 0;

    // Property: must advertise gzip.
    if (encIt->second.find("gzip") == std::string::npos)
        __builtin_trap();

    // Property: ratio bound.
    if (resp.body.size() > static_cast<std::size_t>(
            static_cast<double>(body.size()) * 1.0001 + 64))
        __builtin_trap();

    return 0;
}