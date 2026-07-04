#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <chrono>
#include <cstdio>
#include <string>

#include <Baldr/Http/TraceContext.hpp>
#include <Baldr/Middleware/IMiddleware.hpp>

namespace BALDR_NAMESPACE
{

    struct RequestIdOptions
    {
        bool propagateTraceparentResponse  = true;
        bool useTraceIdAsRequestIdFallback = true;
    };

    class RequestIdMiddleware final : public IMiddleware
    {
      public:
        RequestIdMiddleware() = default;
        explicit RequestIdMiddleware(RequestIdOptions opts) : mOptions(opts) {}
        ~RequestIdMiddleware() override = default;

        static constexpr const char* kHeaderName        = "X-Request-ID";
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
        static std::string generate()
        {
            static thread_local std::uint64_t counter = 0;
            const auto                        now = static_cast<std::uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count());
            const auto mix = now ^ (++counter * 0x9E3779B97F4A7C15ULL);
            char       buf[17];
            std::snprintf(buf, sizeof(buf), "%016llx",
                          static_cast<unsigned long long>(mix));
            return buf;
        }

        RequestIdOptions mOptions;
    };

} // namespace BALDR_NAMESPACE