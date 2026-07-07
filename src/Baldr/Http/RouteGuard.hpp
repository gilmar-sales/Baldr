/**
 * @file Http/RouteGuard.hpp
 * @brief Per-route pre-dispatch guards (currently: max body size).
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cstddef>
#include <optional>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/RouteOptions.hpp>
#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Enforce @c RouteOptions::maxBodyBytes against @p request.
     *
     * Returns a populated @c HttpResponse (status @c 413 Payload Too Large)
     * when the request body exceeds the cap or its declared
     * @c Content-Length is above the cap; returns @c std::nullopt when the
     * route has no override, or when the request is within the cap.
     *
     * The check is two-stage: the accumulated @c request.body (parsed) is
     * compared first, then the declared @c Content-Length header. Both
     * sides reject with the same response shape so a client cannot
     * smuggle an oversized payload by lying about length.
     *
     * @param request The request being dispatched.
     * @param options The route's @c RouteOptions.
     * @return A ready-to-send 413 response, or @c std::nullopt.
     */
    inline std::optional<HttpResponse> EnforceMaxBodySize(
        const HttpRequest& request, const RouteOptions& options)
    {
        if (!options.maxBodyBytes.has_value())
            return std::nullopt;

        const auto cap      = *options.maxBodyBytes;
        bool       tooLarge = request.body.size() > cap;

        if (!tooLarge)
        {
            auto clIt = request.headers.find("content-length");
            if (clIt != request.headers.end())
            {
                const auto& raw = clIt->second;
                if (!raw.empty())
                {
                    unsigned long long declared = 0;
                    for (char c : raw)
                    {
                        if (c < '0' || c > '9')
                        {
                            declared = 0;
                            break;
                        }
                        declared = declared * 10ULL +
                                   static_cast<unsigned long long>(c - '0');
                        if (declared > cap)
                        {
                            tooLarge = true;
                            break;
                        }
                    }
                    if (!tooLarge && declared > cap)
                        tooLarge = true;
                }
            }
        }

        if (!tooLarge)
            return std::nullopt;

        HttpResponse response;
        response.version                 = "HTTP/1.1";
        response.statusCode              = StatusCode::PayloadTooLarge;
        response.body                    = "Payload Too Large";
        response.headers["Content-Type"] = "text/plain";
        response.headers["Content-Length"] =
            std::to_string(response.body.size());
        response.headers["Connection"] = "close";
        return response;
    }

} // namespace BALDR_NAMESPACE