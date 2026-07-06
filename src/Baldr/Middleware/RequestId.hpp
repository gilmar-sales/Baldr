/**
 * @file Middleware/RequestId.hpp
 * @brief Middleware that ensures every request/response pair carries an
 *        @c X-Request-ID and propagates W3C @c traceparent state.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <chrono>
#include <cstdio>
#include <string>

#include <Baldr/Hosting/SecureRandom.hpp>
#include <Baldr/Http/TraceContext.hpp>
#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Configuration for @ref RequestIdMiddleware.
     */
    struct RequestIdOptions
    {
        /// When @c true, echo the resolved @c traceparent header on the
        /// response.
        bool propagateTraceparentResponse = true;
        /// When @c true and no @c X-Request-ID was supplied, reuse the trace
        /// ID.
        bool useTraceIdAsRequestIdFallback = true;
    };

    /**
     * @brief Middleware that mints / propagates request IDs and W3C
     *        @c traceparent state.
     *
     * Resolution order for the @c X-Request-ID:
     *   1. Incoming @c X-Request-ID header (verbatim).
     *   2. The trace ID (when @ref
     * RequestIdOptions::useTraceIdAsRequestIdFallback is @c true and a trace
     * context exists).
     *   3. A freshly generated random ID.
     *
     * The chosen ID is echoed back in the response headers.
     */
    class RequestIdMiddleware final : public IMiddleware
    {
      public:
        RequestIdMiddleware() = default;
        explicit RequestIdMiddleware(RequestIdOptions opts) : mOptions(opts) {}
        ~RequestIdMiddleware() override = default;

        /// Canonical request ID header name.
        static constexpr const char* kHeaderName = "X-Request-ID";
        /// Canonical W3C @c traceparent header name.
        static constexpr const char* kTraceparentHeader = "traceparent";

        void Handle(HttpRequest&          request,
                    HttpResponse&         response,
                    const NextMiddleware& next) override
        {
            auto populateTraceContext = [&](const std::string& incomingTraceId,
                                            std::uint8_t       flags,
                                            bool               keepIncoming) {
                request.traceContext.version = 0;
                request.traceContext.traceId =
                    keepIncoming ? incomingTraceId : NewTraceId();
                request.traceContext.spanId     = NewSpanId();
                request.traceContext.traceFlags = flags;
                request.traceContext.valid      = true;
            };

            const auto tpIt = request.headers.find(kTraceparentHeader);
            if (tpIt != request.headers.end() && !tpIt->second.empty())
            {
                TraceContext parsed;
                if (TryParseTraceparent(tpIt->second, parsed))
                {
                    populateTraceContext(parsed.traceId, parsed.traceFlags,
                                         true);
                }
                else
                {
                    request.traceContext.valid = true;
                    request.traceContext =
                        TraceContext { 0, NewTraceId(), NewSpanId(), 0, true };
                }
            }
            else
            {
                request.traceContext.valid = true;
                request.traceContext =
                    TraceContext { 0, NewTraceId(), NewSpanId(), 0, true };
            }

            const auto  ridIt = request.headers.find("x-request-id");
            std::string rid;
            bool        ridExplicit = false;
            if (ridIt != request.headers.end() && !ridIt->second.empty())
            {
                rid         = ridIt->second;
                ridExplicit = true;
            }
            else if (mOptions.useTraceIdAsRequestIdFallback &&
                     request.traceContext.valid)
            {
                rid = request.traceContext.traceId;
            }
            else
            {
                rid = generate();
            }

            request.headers[kHeaderName]  = rid;
            response.headers[kHeaderName] = rid;

            if (mOptions.propagateTraceparentResponse &&
                request.traceContext.valid)
            {
                char out[128];
                std::snprintf(
                    out,
                    sizeof(out),
                    "%02x-%s-%s-%02x",
                    static_cast<unsigned>(request.traceContext.version),
                    request.traceContext.traceId.c_str(),
                    request.traceContext.spanId.c_str(),
                    static_cast<unsigned>(request.traceContext.traceFlags));
                response.headers[kTraceparentHeader] = out;
            }

            next();
        }

      private:
        static std::string generate() { return RandomHex(16); }

        RequestIdOptions mOptions;
    };

} // namespace BALDR_NAMESPACE