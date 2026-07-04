#pragma once

#include <Baldr/Http/TraceContext.hpp>
#include <Baldr/Middleware/IMiddleware.hpp>

#include <chrono>
#include <format>
#include <meta>
#include <string>

#include <Skirnir/Common/Reflection.hpp>

class LoggingMiddleware final : public IMiddleware
{
  public:
    LoggingMiddleware(skr::Arc<skr::Logger<LoggingMiddleware>> logger) :
        mLogger(logger)
    {
    }

    ~LoggingMiddleware() = default;

    static std::string FormatRequestLine(const HttpRequest& request)
    {
        const auto  method = refl::enum_to_string(request.method);
        std::string suffix;
        appendTraceSuffix(request.traceContext, suffix);
        return std::format("Request - '{}' '{}' '{}'{}", request.version,
                           method, request.path, suffix);
    }

    static std::string FormatResponseLine(const HttpRequest&        request,
                                          const HttpResponse&       response,
                                          std::chrono::microseconds duration)
    {
        const auto  method = refl::enum_to_string(request.method);
        std::string suffix;
        appendTraceSuffix(request.traceContext, suffix);
        return std::format("Response - {} '{}' '{}' - {} - {}{}",
                           static_cast<int>(response.statusCode), method,
                           request.path, duration, request.clientIp, suffix);
    }

    void Handle(HttpRequest&          request,
                HttpResponse&         response,
                const NextMiddleware& next) override
    {
        mLogger->LogInformation("{}", FormatRequestLine(request));

        auto begin = std::chrono::system_clock::now();

        next();

        auto end = std::chrono::system_clock::now();

        mLogger->LogInformation(
            "{}",
            FormatResponseLine(
                request, response,
                std::chrono::duration_cast<std::chrono::microseconds>(
                    end - begin)));
    }

  private:
    static void appendTraceSuffix(const Baldr::TraceContext& tc,
                                  std::string&               out)
    {
        if (!tc.valid)
        {
            return;
        }
        out += " trace=";
        out += tc.traceId;
        if (tc.sampled())
        {
            out += " span=";
            out += tc.spanId;
        }
    }

    skr::Arc<skr::Logger<LoggingMiddleware>> mLogger;
};