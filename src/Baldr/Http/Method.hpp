/**
 * @file Http/Method.hpp
 * @brief Enumeration of HTTP request methods supported by the router.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief HTTP/1.1 request methods recognised by Baldr.
     *
     * The integer value is used internally for hash-table bucketing and is
     * not significant; use the symbolic names. Round-tripping through
     * simdjson or HTTP wire format is handled by the framework.
     */
    enum class HttpMethod : int
    {
        Get,     ///< @c GET
        Post,    ///< @c POST
        Put,     ///< @c PUT
        Delete,  ///< @c DELETE
        Patch,   ///< @c PATCH
        Options, ///< @c OPTIONS (CORS pre-flight)
        Head,    ///< @c HEAD
        Trace,   ///< @c TRACE
        Connect  ///< @c CONNECT (not routed by default)
    };

} // namespace BALDR_NAMESPACE
