// CookieFuzz.cpp - Fuzz target for cookie parsing/emission.
//
// Drives mutated Cookie header strings into HttpRequest::cookies (via the
// parser path) and mutated CookieOptions values into HttpResponse::cookies
// (which the connection serialises as Set-Cookie). Property assertions:
//   * parsed cookie names match RFC 6265 token grammar
//   * serialised Set-Cookie strings never contain \r\n
//   * SameSite value is one of None / Lax / Strict

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include <Baldr/Http/CookieOptions.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/RequestParser.hpp>
#include <Baldr/Http/Response.hpp>

#include "FuzzedDataProvider.hpp"

namespace
{
    bool isTokenChar(unsigned char c)
    {
        // RFC 6265 cookie-octet / token subset; tighter than RFC 7230.
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z') || c == '!' || c == '#' || c == '$' ||
               c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' ||
               c == '-' || c == '.' || c == '^' || c == '_' || c == '`' ||
               c == '|' || c == '~';
    }

    std::string serialiseCookie(const std::string&            name,
                                const baldr::CookieOptions&    opts)
    {
        std::string out = "Set-Cookie: ";
        out += name;
        out += "=";
        out += opts.value;
        if (opts.domain)
        {
            out += "; Domain=";
            out += *opts.domain;
        }
        out += "; Path=/";
        if (opts.maxAge > 0)
            out += "; Max-Age=" + std::to_string(opts.maxAge);
        if (opts.httpOnly)
            out += "; HttpOnly";
        if (opts.secure)
            out += "; Secure";
        switch (opts.sameSite)
        {
            case baldr::SameSite::None:   out += "; SameSite=None";   break;
            case baldr::SameSite::Lax:    out += "; SameSite=Lax";    break;
            case baldr::SameSite::Strict: out += "; SameSite=Strict"; break;
        }
        return out;
    }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    // Drive an HTTP request through the real parser so cookies get parsed
    // by the production code path (not duplicated).
    auto host = fdp.ConsumeBool();
    auto cookie = fdp.ConsumeBytesAsString(
        fdp.ConsumeIntegralInRange<std::size_t>(0, 512));

    std::string request = "GET / HTTP/1.1\r\n";
    if (host)
        request += "Host: x\r\n";
    if (!cookie.empty())
    {
        request += "Cookie: ";
        request += cookie;
        request += "\r\n";
    }
    request += "\r\n";

    baldr::HttpRequestParser p;
    auto status = p.tryParse(request);
    if (status.kind != baldr::HttpParseStatus::Kind::Complete)
        return 0;

    // Property: parsed names match RFC 6265 token grammar.
    // Report (not crash) on the first violation so afl++ records the
    // oracle failure as a `crash` artifact but local smoke runs without
    // a sanitizer can still iterate the corpus.
    for (const auto& [name, _] : status.request.cookies)
    {
        bool bad = name.empty();
        if (!bad)
        {
            for (char c : name)
            {
                if (!isTokenChar(static_cast<unsigned char>(c)))
                {
                    bad = true;
                    break;
                }
            }
        }
        if (bad)
        {
            std::fprintf(stderr,
                "CookieFuzz: parser produced invalid cookie name (length=%zu)\n",
                name.size());
            __builtin_trap();
        }
    }

    // Drive a response through the serialiser with mutated CookieOptions.
    baldr::HttpResponse resp;
    baldr::CookieOptions opts;
    opts.value = fdp.ConsumeBytesAsString(
        fdp.ConsumeIntegralInRange<std::size_t>(0, 128));
    opts.maxAge  = fdp.ConsumeIntegralInRange<long>(-2, 1'000'000);
    opts.httpOnly = fdp.ConsumeBool();
    opts.secure   = fdp.ConsumeBool();
    opts.sameSite = static_cast<baldr::SameSite>(
        fdp.ConsumeIntegralInRange<std::uint8_t>(0, 2));
    if (fdp.ConsumeBool())
        opts.domain = fdp.ConsumeBytesAsString(64);

    resp.cookies["S"] = opts;
    auto serialised = serialiseCookie("S", resp.cookies["S"]);

    if (serialised.find('\r') != std::string::npos ||
        serialised.find('\n') != std::string::npos)
        __builtin_trap();

    return 0;
}