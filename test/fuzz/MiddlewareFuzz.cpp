// MiddlewareFuzz.cpp - Fuzz target for the built-in middleware stack.
//
// Drives a small request through Cors, Csrf, SecurityHeaders,
// RequestId, and ExceptionHandler. Property assertions:
//   * Gap 3.7: the response body in non-debug builds never contains the
//     substring of a `std::exception::what()` we threw below.
//   * Status is always a value in {200, 204, 400, 403, 500}.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <Baldr/Http/CookieOptions.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Middleware/Cors.hpp>
#include <Baldr/Middleware/Csrf.hpp>
#include <Baldr/Middleware/ExceptionHandler.hpp>
#include <Baldr/Middleware/IMiddleware.hpp>
#include <Baldr/Middleware/RequestId.hpp>
#include <Baldr/Middleware/SecurityHeaders.hpp>

#include "FuzzAssert.hpp"
#include "FuzzedDataProvider.hpp"

namespace
{
    constexpr std::array<baldr::HttpMethod, 5> kUnsafeOrSafe {
        baldr::HttpMethod::Get,    baldr::HttpMethod::Post,
        baldr::HttpMethod::Put,    baldr::HttpMethod::Options,
        baldr::HttpMethod::Delete,
    };

    void runChain(baldr::HttpRequest& req, baldr::HttpResponse& resp)
    {
        baldr::CorsMiddleware             cors;
        baldr::SecurityHeadersMiddleware  sec;
        baldr::RequestIdMiddleware        rid;
        baldr::CsrfMiddleware             csrf;
        baldr::ExceptionHandlerMiddleware exc({ .includeDetailsInDev = false });

        // Build the chain from inside-out: rid -> csrf -> sec -> cors -> exc.
        baldr::NextMiddleware terminal = [&]() {
            auto it = req.headers.find("x-bug");
            if (it != req.headers.end() &&
                it->second.find("throw") != std::string::npos)
            {
                throw std::runtime_error("secret-db-password=hunter2 leaked!");
            }
            resp.statusCode              = baldr::StatusCode::OK;
            resp.body                    = "ok";
            resp.headers["Content-Type"] = "text/plain";
        };
        baldr::NextMiddleware ridChain = [&, terminal]() {
            rid.Handle(req, resp, terminal);
        };
        baldr::NextMiddleware csrfChain = [&, ridChain]() {
            csrf.Handle(req, resp, ridChain);
        };
        baldr::NextMiddleware secChain = [&, csrfChain]() {
            sec.Handle(req, resp, csrfChain);
        };
        baldr::NextMiddleware corsChain = [&, secChain]() {
            cors.Handle(req, resp, secChain);
        };
        exc.Handle(req, resp, corsChain);
    }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzRecorder rec({ data, size });
    baldr::fuzz::g_active_recorder = &rec;
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    baldr::HttpRequest  req;
    baldr::HttpResponse resp(req);
    req.method = fdp.PickValueInArray(kUnsafeOrSafe);
    req.path   = "/" + fdp.ConsumeRandomLengthString(64);
    BDR_RECORD(fdp, req.path, "path");
    req.headers["host"] = "x";

    auto hcount = fdp.ConsumeIntegralInRange<std::size_t>(0, 6);
    for (std::size_t i = 0; i < hcount; ++i)
    {
        auto key = fdp.ConsumeBytesAsString(
            fdp.ConsumeIntegralInRange<std::size_t>(1, 16));
        auto val = fdp.ConsumeBytesAsString(
            fdp.ConsumeIntegralInRange<std::size_t>(0, 32));
        BDR_RECORD(fdp, key, "hdr_key");
        BDR_RECORD(fdp, val, "hdr_value");
        // Lowercase the key (parser behavior) so middleware lookup matches.
        for (auto& c : key)
        {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c + 32);
        }
        if (!key.empty() && !req.headers.contains(key))
            req.headers[key] = val;
    }

    // Maybe carry a CSRF cookie.
    if (fdp.ConsumeBool())
    {
        auto xsrf = fdp.ConsumeBytesAsString(32);
        BDR_RECORD(fdp, xsrf, "xsrf_token");
        req.cookies["XSRF-TOKEN"] = xsrf;
    }

    runChain(req, resp);

    // Gap 3.7: secret must not leak.
    BDR_ASSERT(rec, resp.body.find("hunter2") == std::string::npos,
               "secret leak in body (hunter2)");
    BDR_ASSERT(rec, resp.body.find("secret") == std::string::npos,
               "secret leak in body (secret)");

    // Status sanity.
    switch (resp.statusCode)
    {
        case baldr::StatusCode::OK:
        case baldr::StatusCode::NoContent:
        case baldr::StatusCode::BadRequest:
        case baldr::StatusCode::Forbidden:
        case baldr::StatusCode::InternalServerError:
            break;
        default:
            // 200, 204, 400, 403, 500 are the only ones this chain produces.
            BDR_ASSERT(rec, false, "status code outside {200,204,400,403,500}");
    }

    baldr::fuzz::g_active_recorder = nullptr;
    return 0;
}