/**
 * @file Http/Server.hpp
 * @brief Trantor-based HTTP/1.1 server that accepts connections and
 *        dispatches them through the route/middleware pipeline.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <atomic>
#include <memory>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Application/InFlightTracker.hpp>
#include <Baldr/Http/ServerOptions.hpp>

namespace BALDR_NAMESPACE {

/**
 * @brief Long-running HTTP/1.1 server backed by trantor's event loop.
 *
 * Owns the listening socket, an acceptor pool and per-connection
 * @c HttpConnection objects. Lifetime is controlled via @ref Run and
 * @ref Stop; @ref Run blocks until @ref Stop is invoked or the process
 * is signalled.
 */
class HttpServer
{
  public:
    /**
     * @brief Construct a server.
     *
     * @param httpServerOptions Listening port, thread count, keep-alive
     *                          settings, shutdown timeout.
     * @param serviceProvider   Shared service provider (router,
     *                          middleware provider, parsers, loggers).
     * @param logger            Logger tagged for @c HttpServer.
     * @param inFlightTracker   Process-wide in-flight handler counter used
     *                          for graceful drain on shutdown.
     */
    HttpServer(const skr::Arc<HttpServerOptions>&       httpServerOptions,
               const skr::Arc<skr::ServiceProvider>&    serviceProvider,
               const skr::Arc<skr::Logger<HttpServer>>& logger,
               const skr::Arc<InFlightTracker>&         inFlightTracker);

    ~HttpServer();

    HttpServer(const HttpServer&)            = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /**
     * @brief Start the listener and block until @ref Stop is called.
     */
    void Run();

    /**
     * @brief Stop accepting new connections, drain in-flight handlers
     *        (subject to @c HttpServerOptions::gracefulShutdownTimeoutSeconds),
     *        then return.
     */
    void Stop();

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace BALDR_NAMESPACE