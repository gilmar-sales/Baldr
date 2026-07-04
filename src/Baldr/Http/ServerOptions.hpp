#pragma once

#include <thread>

struct HttpServerOptions
{
    // NOTE: TLS is not currently supported. Baldr builds with
    // TRANTOR_USE_TLS=none, so https cannot be served. Adding TLS would
    // require either enabling OpenSSL in trantor (and wiring certificate
    // paths here) or terminating TLS upstream (e.g. via a reverse proxy).
    short port        = 8080;
    int   threadCount = static_cast<int>(std::thread::hardware_concurrency());

    // Maximum number of requests served over a single TCP connection
    // before the connection is force-closed. Mitigates slowloris-style
    // keep-alive abuse. Set to 0 to disable the cap.
    int maxRequestsPerConnection = 1000;

    // When true, HTTP/1.1 connections without an explicit `Connection:
    // close` header are kept alive (subject to the per-connection cap
    // above). When false, every connection is closed after a single
    // response. The framework always honours an explicit `Connection:
    // close` from the peer and always closes HTTP/1.0 connections.
    bool enableHttp11KeepAlive = true;

    // On `Stop()`, the server stops accepting new connections and waits
    // up to this many seconds for in-flight handlers to complete
    // before force-closing remaining connections. Set to a negative
    // value to skip the drain and force-close immediately.
    int gracefulShutdownTimeoutSeconds = 30;
};
