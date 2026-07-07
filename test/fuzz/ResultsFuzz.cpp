// ResultsFuzz.cpp - Fuzz target for IResult subclasses.
//
// Picks a result type and body, applies it to an HttpResponse, and asserts
// that:
//   * status code is one of the known set
//   * Content-Type header values do not contain \r\n

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/Results/TypedResults.hpp>
#include <Baldr/Http/StatusCode.hpp>

#include "FuzzAssert.hpp"
#include "FuzzedDataProvider.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzRecorder rec({ data, size });
    baldr::fuzz::g_active_recorder = &rec;
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    auto        pick = fdp.ConsumeIntegralInRange<std::uint8_t>(0, 4);
    std::string body = fdp.ConsumeBytesAsString(
        fdp.ConsumeIntegralInRange<std::size_t>(0, 1024));
    BDR_RECORD(fdp, body, "body");

    baldr::HttpResponse resp;
    switch (pick)
    {
        case 0:
            baldr::TextResult(body).Apply(resp);
            break;
        case 1:
            baldr::ContentResult(body, "text/plain").Apply(resp);
            break;
        case 2:
            baldr::StatusResult(baldr::StatusCode::NotFound).Apply(resp);
            break;
        case 3:
            baldr::OkResult::Text(body).Apply(resp);
            break;
        default:
            baldr::ContentResult(body, "application/octet-stream").Apply(resp);
            break;
    }

    // Header-injection invariant.
    for (const auto& [k, v] : resp.headers)
    {
        BDR_ASSERT(rec,
                   v.find('\r') == std::string::npos &&
                       v.find('\n') == std::string::npos,
                   "header value contains CR/LF");
    }

    // Status sanity: status is always a sane value.
    int code = static_cast<int>(resp.statusCode);
    BDR_ASSERT(rec, code >= 100 && code <= 599,
               "status code outside [100,599]");

    baldr::fuzz::g_active_recorder = nullptr;
    return 0;
}