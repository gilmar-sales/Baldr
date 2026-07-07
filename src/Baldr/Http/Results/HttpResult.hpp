/**
 * @file Http/Results/HttpResult.hpp
 * @brief Lightweight result envelope used by the request parser and
 *        other internal helpers to communicate either a value or an
 *        error back to the caller.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Either-or envelope holding a deserialised value or an error
     *        description with an HTTP status code.
     *
     * Used internally by helpers that may fail without throwing (e.g. the
     * request parser). Callers check @c success before reading @c value.
     */
    template <typename TValue>
    struct HttpResult
    {
        /// @c true when @c value is populated; @c false when @c error is.
        bool success = false;
        /// Human-readable error message (empty on success).
        std::string error;
        /// Suggested HTTP status code (set for both success and failure paths).
        StatusCode statusCode = StatusCode::InternalServerError;
        /// Decoded value (meaningful only when @c success is true).
        TValue value;
    };

} // namespace BALDR_NAMESPACE
