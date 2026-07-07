/**
 * @file Middleware/Logging.hpp
 * @brief Access-log middleware that emits one structured line per
 *        request, including the resolved W3C trace context.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Http/TraceContext.hpp>
#include <Baldr/Middleware/IMiddleware.hpp>

#include <chrono>
#include <format>
#include <meta>
#include <string>

#include <Skirnir/Common/Reflection.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Middleware that writes a one-line request log on the way in
     *        and a one-line response log (with duration and status) on
     *        the way out.
     */
    class LoggingMiddleware final : public IMiddleware
    {
      public:
        /**
         * @brief Construct with a logger tagged for this middleware type.
         */
        LoggingMiddleware(skr::Arc<skr::Logger<LoggingMiddleware>> logger) :
            mLogger(logger)
        {
        }

        ~LoggingMiddleware() = default;

        /**
         * @brief Format the request-side access log line.
         *
         * Format: @c Request - '<version>' '<method>' '<path>' [trace=<id>
         * [span=<id>]]
         */
        static std::string FormatRequestLine(const HttpRequest& request)
        {
            const auto  method = refl::enum_to_string(request.method);
            std::string suffix;
            appendTraceSuffix(request.traceContext, suffix);
            return std::format("Request - '{}' '{}' '{}'{}", request.version,
                               method, request.path, suffix);
        }

        /**
         * @brief Format the response-side access log line.
         *
         * Format: @c Response - <status> '<method>' '<path>' - <micros>us -
         * <clientIp> [trace=<id>]
         */
        static std::string FormatResponseLine(
            const HttpRequest&        request,
            const HttpResponse&       response,
            std::chrono::microseconds duration)
        {
            const auto  method = refl::enum_to_string(request.method);
            std::string suffix;
            appendTraceSuffix(request.traceContext, suffix);
            return std::format(
                "Response - {} '{}' '{}' - {} - {}{}",
                static_cast<int>(response.statusCode), method, request.path,
                duration, request.clientIp, suffix);
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
        /**
         * @brief Append @c trace=<id> (and @c span=<id> when sampled) to
         *        @p out when @p tc is valid. No-op when the request had no
         *        @c traceparent.
         */
        static void appendTraceSuffix(const TraceContext& tc, std::string& out)
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

} // namespace BALDR_NAMESPACE