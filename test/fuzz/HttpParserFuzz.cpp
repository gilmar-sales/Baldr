// HttpParserFuzz.cpp - Fuzz target for baldr::HttpRequestParser.
//
// Inputs are FDP-shaped: a few control bytes drive a small struct (method
// choice, body size, header count), then the remaining bytes are the raw
// wire payload fed to HttpRequestParser::tryParse. Property assertions
// (in src/Baldr/Http/RequestParser.cpp invariants) catch OOB index reads,
// Content-Length overflow, and body-cap bypass.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/RequestParser.hpp>
#include <Baldr/Http/StatusCode.hpp>

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

    const char* methodToVerb(baldr::HttpMethod m)
    {
        switch (m)
        {
            case baldr::HttpMethod::Get:     return "GET";
            case baldr::HttpMethod::Post:    return "POST";
            case baldr::HttpMethod::Put:     return "PUT";
            case baldr::HttpMethod::Delete:  return "DELETE";
            case baldr::HttpMethod::Patch:   return "PATCH";
            case baldr::HttpMethod::Options: return "OPTIONS";
            case baldr::HttpMethod::Head:    return "HEAD";
            case baldr::HttpMethod::Trace:   return "TRACE";
            case baldr::HttpMethod::Connect: return "CONNECT";
        }
        return "GET";
    }

    // Build a mostly-well-formed request around the mutated payload so the
    // parser reaches the regions afl++ wants to explore.
    std::string buildScaffold(baldr::fuzz::FuzzedDataProvider& fdp,
                              std::string_view                body)
    {
        auto method = fdp.PickValueInArray(kMethods);
        auto hasHost = fdp.ConsumeBool();
        auto host = fdp.ConsumeBytesAsString(
            fdp.ConsumeIntegralInRange<std::size_t>(0, 64));
        auto path = fdp.ConsumeBytesAsString(
            fdp.ConsumeIntegralInRange<std::size_t>(0, 64));

        std::string req;
        req += methodToVerb(method);
        req += " /";
        req += path;
        req += " HTTP/1.1\r\n";

        if (hasHost)
        {
            req += "Host: ";
            req += host.empty() ? "x" : host;
            req += "\r\n";
        }

        if (body.empty())
        {
            req += "\r\n";
        }
        else
        {
            std::string cl = "Content-Length: ";
            cl += std::to_string(body.size());
            cl += "\r\n\r\n";
            req += cl;
            req.append(body.data(), body.size());
        }
        return req;
    }
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size)
{
    baldr::fuzz::FuzzedDataProvider fdp(data, size);

    // Pull a small scaffold, then feed the parser. We pick a tight max
    // body size to exercise the cap path.
    baldr::HttpRequestParser parser;
    parser.maxBodySize = 1024;

    std::string body = fdp.ConsumeRandomLengthString(2048);
    std::string request = buildScaffold(fdp, std::string_view(body));

    auto status = parser.tryParse(std::string_view(request));

    // Property assertions.
    if (status.kind == baldr::HttpParseStatus::Kind::Complete)
    {
        // Body must never exceed maxBodySize.
        if (status.request.body.size() > parser.maxBodySize)
            __builtin_trap();
        // Path must be non-empty after stripping query.
        if (status.request.path.empty())
            __builtin_trap();
        // Method must be one of the recognized verbs.
        switch (status.request.method)
        {
            case baldr::HttpMethod::Get:
            case baldr::HttpMethod::Post:
            case baldr::HttpMethod::Put:
            case baldr::HttpMethod::Delete:
            case baldr::HttpMethod::Patch:
            case baldr::HttpMethod::Options:
            case baldr::HttpMethod::Head:
            case baldr::HttpMethod::Trace:
            case baldr::HttpMethod::Connect:
                break;
            default:
                __builtin_trap();
        }
    }

    return 0;
}