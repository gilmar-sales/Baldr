/**
 * @file Http/ServerOptions.hpp
 * @brief Configuration object passed to @c HttpServer at construction.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <thread>

namespace BALDR_NAMESPACE {

/**
 * @brief Static configuration for @c HttpServer.
 *
 * @note TLS is not currently supported. Baldr builds with
 *       @c TRANTOR_USE_TLS=none, so https cannot be served. Adding TLS
 *       would require either enabling OpenSSL in trantor (and wiring
 *       certificate paths here) or terminating TLS upstream (e.g. via
 *       a reverse proxy).
 */
struct HttpServerOptions
{
    /// TCP port to listen on.
    short port        = 8080;
    /// Number of I/O threads; defaults to @c std::thread::hardware_concurrency().
    int   threadCount = static_cast<int>(std::thread::hardware_concurrency());

    /**
     * @brief Maximum requests served over a single TCP connection before
     *        it is force-closed. Mitigates slowloris-style keep-alive
     *        abuse. Set to 0 to disable the cap.
     */
    int maxRequestsPerConnection = 1000;

    /**
     * @brief Enable HTTP/1.1 keep-alive for connections without an
     *        explicit @c Connection: close header.
     *
     * When @c false, every connection is closed after a single response.
     * The framework always honours an explicit @c Connection: close from
     * the peer and always closes HTTP/1.0 connections.
     */
    bool enableHttp11KeepAlive = true;

    /**
     * @brief On @c Stop(), wait up to this many seconds for in-flight
     *        handlers to complete before force-closing remaining
     *        connections. Negative values skip the drain and close
     *        immediately.
     */
    int gracefulShutdownTimeoutSeconds = 30;
};

} // namespace BALDR_NAMESPACE
