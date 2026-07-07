/**
 * @file Application/HealthChecks.hpp
 * @brief Register /healthz and /readyz style health-check endpoints.
 *
 * @note This header is an internal adapter used by
 *       @c WebApplication::MapHealthChecks. End-user code registers
 *       probes through the @c IHealthCheck interface; this struct just
 *       snapshots resolved instances at registration time so the
 *       request-thread handler does not need to re-resolve services.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <functional>
#include <string>
#include <vector>

#include <Baldr/Application/HealthCheckResult.hpp>
#include <Baldr/Http/Request.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief A single named health predicate, snapshot at registration time.
     *
     * @c check returns a @ref HealthCheckResult describing severity plus
     * optional @c description, @c error, and pre-serialized @c data.
     */
    struct HealthCheckRegistration
    {
        std::string name; ///< Short identifier surfaced in the JSON body.
        std::function<HealthCheckResult(const HttpRequest&)>
            check; ///< Predicate invoked per request.
    };

} // namespace BALDR_NAMESPACE