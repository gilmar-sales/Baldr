/**
 * @file Application/HealthChecks.hpp
 * @brief Register /healthz and /readyz style health-check endpoints.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <functional>
#include <string>
#include <vector>

#include <Baldr/Http/Request.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief A single named health predicate.
     *
     * The predicate is invoked synchronously on the request thread, so it
     * must be cheap and non-blocking. Long-running probes (e.g. database
     * pings) should cache their last known result and use a short timeout.
     *
     * @c true means healthy; @c false means unhealthy.
     */
    struct HealthCheckRegistration
    {
        std::string name; ///< Short identifier surfaced in the JSON body.
        std::function<bool(const HttpRequest&)>
            check; ///< Predicate invoked per request.
    };

} // namespace BALDR_NAMESPACE