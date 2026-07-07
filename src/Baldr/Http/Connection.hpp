/**
 * @file Http/Connection.hpp
 * @brief Per-TCP-connection HTTP/1.1 handler. Owns the parser state and
 *        the read accumulator, and drives the middleware pipeline.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include <Skirnir/Common.hpp>
#include <Skirnir/Common/Namespace.hpp>
#include <trantor/net/TcpConnection.h>
#include <trantor/utils/MsgBuffer.h>

#include <Baldr/Application/InFlightTracker.hpp>
#include <Baldr/Hosting/StringHelpers.hpp>
#include <Baldr/Http/Method.hpp>
#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/RequestParser.hpp>
#include <Baldr/Http/Response.hpp>
#include <Baldr/Http/Router.hpp>
#include <Baldr/Http/ServerOptions.hpp>
#include <Baldr/Http/StatusCode.hpp>
#include <Baldr/Middleware/MiddlewareProvider.hpp>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Owns the parser, accumulator and middleware chain for a single
     *        client connection.
     *
     * Constructed by @c HttpServer when a new connection is accepted; the
     * trantor event loop dispatches incoming bytes to @ref onMessage.
     */
    class HttpConnection
    {
      public:
        /**
         * @brief Bind a new connection to the shared service graph.
         *
         * @param serviceProvider The application-wide DI container.
         * @param conn            Trantor's connection pointer.
         */
        HttpConnection(const skr::Arc<skr::ServiceProvider>& serviceProvider,
                       const trantor::TcpConnectionPtr&      conn) :
            mServiceProvider(serviceProvider),
            mRouter(serviceProvider->GetService<Router>()),
            mMiddlewareProvider(
                serviceProvider->GetService<MiddlewareProvider>()),
            mLogger(serviceProvider->GetService<skr::Logger<HttpConnection>>()),
            mParser(serviceProvider->GetService<HttpRequestParser>()),
            mServerOptions(serviceProvider->GetService<HttpServerOptions>()),
            mInFlightTracker(serviceProvider->GetService<InFlightTracker>()),
            mConnection(conn), mClientIp(conn->peerAddr().toIp())
        {
        }

        /**
         * @brief Append @p buffer's bytes to the per-connection accumulator
         *        and dispatch every complete request contained in it.
         *
         * @param buffer Bytes just received from the peer.
         */
        void onMessage(trantor::MsgBuffer* buffer);

        /**
         * @brief Hard cap on the per-connection read accumulator. If a single
         *        request would push the accumulator past this limit the
         *        connection is closed.
         */
        static constexpr std::size_t kMaxAccumulatorBytes = 10 * 1024 * 1024;

        /**
         * @brief Execute the middleware chain around a single request.
         *
         * @param factories      Snapshot of middleware factories from the
         *                       sealed @c MiddlewareProvider.
         * @param scopedProvider Per-request DI scope used to construct each
         *                       middleware instance.
         * @param request        Request being dispatched (mutable so middleware
         *                       may attach context).
         * @param response       Response populated by the chain.
         * @param finalHandler   Terminal handler (the route callback).
         */
        static void runMiddlewareChain(
            MiddlewareFactoryList&                factories,
            const skr::Arc<skr::ServiceProvider>& scopedProvider,
            HttpRequest&                          request,
            HttpResponse&                         response,
            const RouteHandler&                   finalHandler);

        /**
         * @brief Validate that every outgoing header name is a valid RFC 9110
         *        token and every value is free of CR/LF.
         *
         * Returns @c false (and @p firstBadName / @p firstBadValue describe
         * the violation) when a header would inject a new line into the
         * response wire format (HTTP response splitting, CWE-93).
         */
        static bool ValidateResponseHeaders(
            const std::unordered_map<std::string, std::string>&   headers,
            const std::unordered_map<std::string, CookieOptions>& cookies,
            std::string&                                          firstBadName,
            std::string&                                          firstBadValue)
        {
            for (const auto& [name, value] : headers)
            {
                if (name == "Connection")
                    continue;
                if (!isValidHeaderName(name) || containsCrlf(value))
                {
                    firstBadName  = name;
                    firstBadValue = value;
                    return false;
                }
            }
            for (const auto& [name, opts] : cookies)
            {
                if (containsCrlf(name) || containsCrlf(opts.value) ||
                    (opts.domain.has_value() && containsCrlf(*opts.domain)))
                {
                    firstBadName  = name;
                    firstBadValue = opts.value;
                    return false;
                }
            }
            return true;
        }

      private:
        /**
         * @brief Process a parsed request: match a route, run middleware,
         *        serialise the response, and write it to the wire.
         *
         * Wraps the entire pipeline in @c try/@c catch so any handler
         * exception is converted to a 500. Increments @c mRequestCount
         * and the in-flight tracker.
         *
         * @param request The fully-parsed request (headers, body, cookies).
         */
        void handle(HttpRequest request);

        /**
         * @brief Build and send a one-shot error response with
         *        @c Connection: close.
         *
         * @param statusCode HTTP status to send.
         * @param body       Plain-text body sent to the client.
         */
        void sendErrorResponse(StatusCode statusCode, const std::string& body);

        /**
         * @brief Serialise a response header block + body and write it to
         *        the socket. Rejects any header carrying CR/LF (CWE-93).
         *
         * @param response        Response to serialise.
         * @param closeConnection When @c true appends
         *                        @c "Connection: close" and force-closes
         *                        the underlying trantor connection.
         */
        void sendResponse(const HttpResponse& response, bool closeConnection);

        /**
         * @brief Send an HTTP/1.1 chunked response from an
         *        @c IStreamingResult producer.
         *
         * @param result   The streaming body source.
         * @param version  Wire version (e.g. @c "HTTP/1.1").
         * @param cookies  Cookies to attach as @c Set-Cookie headers.
         */
        void sendStreamingResponse(
            const IStreamingResult&                               result,
            const std::string&                                    version,
            const std::unordered_map<std::string, CookieOptions>& cookies);

        /**
         * @brief Parse a query string into @c HttpRequest::query.
         *
         * Tolerant of stray whitespace around @c = and of missing values
         * (treated as the empty string). Percent-encoded sequences are
         * decoded; malformed escapes are silently skipped rather than
         * rejected — the parser has already enforced overall shape.
         */
        static void parseQuery(std::string_view query, HttpRequest& request)
        {
            std::size_t pos = 0;
            while (pos < query.size())
            {
                std::size_t amp = query.find('&', pos);
                if (amp == std::string_view::npos)
                    amp = query.size();
                std::string_view part = query.substr(pos, amp - pos);
                pos                  = amp + 1;
                if (part.empty())
                    continue;
                std::size_t eq = part.find('=');
                std::string_view keySv =
                    (eq == std::string_view::npos) ? part : part.substr(0, eq);
                std::string_view valSv =
                    (eq == std::string_view::npos)
                        ? std::string_view {}
                        : part.substr(eq + 1);
                while (!keySv.empty() &&
                       (keySv.front() == ' ' || keySv.front() == '\t'))
                    keySv.remove_prefix(1);
                while (!keySv.empty() &&
                       (keySv.back() == ' ' || keySv.back() == '\t'))
                    keySv.remove_suffix(1);
                while (!valSv.empty() &&
                       (valSv.front() == ' ' || valSv.front() == '\t'))
                    valSv.remove_prefix(1);
                while (!valSv.empty() &&
                       (valSv.back() == ' ' || valSv.back() == '\t'))
                    valSv.remove_suffix(1);
                if (eq == std::string_view::npos)
                {
                    auto k = decode_path(keySv);
                    if (k)
                        request.query.emplace(std::move(*k), std::string {});
                }
                else
                {
                    auto k = decode_path(keySv);
                    auto v = decode_path(valSv);
                    if (k && v)
                        request.query.emplace(std::move(*k), std::move(*v));
                }
            }
        }

        /**
         * @brief Bridge for callers that want a free-function-style reason
         *        phrase lookup. Forwards to @c reasonPhraseFor in
         *        @c StatusCode.
         */
        static std::string_view reasonPhrase(StatusCode status)
        {
            return reasonPhraseFor(status);
        }

        skr::Arc<skr::ServiceProvider>        mServiceProvider;
        skr::Arc<Router>                      mRouter;
        skr::Arc<MiddlewareProvider>          mMiddlewareProvider;
        skr::Arc<skr::Logger<HttpConnection>> mLogger;
        skr::Arc<HttpServerOptions>           mServerOptions;
        skr::Arc<InFlightTracker>             mInFlightTracker;
        trantor::TcpConnectionPtr             mConnection;
        std::string                           mClientIp;
        std::string                           mAccumulator;
        skr::Arc<HttpRequestParser>           mParser;
        int                                   mRequestCount { 0 };
    };

} // namespace BALDR_NAMESPACE
