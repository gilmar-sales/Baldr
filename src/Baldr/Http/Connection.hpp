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

      private:
        void handle(HttpRequest request);

        void sendErrorResponse(StatusCode statusCode, const std::string& body);

        void sendResponse(const HttpResponse& response, bool closeConnection);

        void sendStreamingResponse(
            const IStreamingResult&                               result,
            const std::string&                                    version,
            const std::unordered_map<std::string, CookieOptions>& cookies);

        static HttpMethod parseMethod(std::string_view method)
        {
            if (method == "GET")
                return HttpMethod::Get;
            if (method == "POST")
                return HttpMethod::Post;
            if (method == "PUT")
                return HttpMethod::Put;
            if (method == "DELETE")
                return HttpMethod::Delete;
            if (method == "PATCH")
                return HttpMethod::Patch;
            if (method == "OPTIONS")
                return HttpMethod::Options;
            if (method == "HEAD")
                return HttpMethod::Head;
            if (method == "TRACE")
                return HttpMethod::Trace;
            if (method == "CONNECT")
                return HttpMethod::Connect;
            return HttpMethod::Get;
        }

        static std::string toLowerAscii(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
            {
                if (c >= 'A' && c <= 'Z')
                    out.push_back(static_cast<char>(c + 32));
                else
                    out.push_back(c);
            }
            return out;
        }

        static void parseQuery(std::string_view query, HttpRequest& request)
        {
            size_t pos = 0;
            while (pos < query.size())
            {
                size_t amp = query.find('&', pos);
                if (amp == std::string_view::npos)
                    amp = query.size();
                std::string_view part = query.substr(pos, amp - pos);
                if (!part.empty())
                {
                    size_t eq = part.find('=');
                    if (eq == std::string_view::npos)
                    {
                        request.query.emplace(std::string(part), "");
                    }
                    else
                    {
                        auto k = decode_path(std::string(part.substr(0, eq)));
                        auto v = decode_path(std::string(part.substr(eq + 1)));
                        if (k && v)
                            request.query.emplace(std::move(*k), std::move(*v));
                    }
                }
                pos = amp + 1;
            }
        }

        static const char* reasonPhrase(StatusCode status)
        {
            switch (status)
            {
                case StatusCode::OK:
                    return "OK";
                case StatusCode::Created:
                    return "Created";
                case StatusCode::Accepted:
                    return "Accepted";
                case StatusCode::NoContent:
                    return "No Content";
                case StatusCode::MovedPermanently:
                    return "Moved Permanently";
                case StatusCode::Found:
                    return "Found";
                case StatusCode::SeeOther:
                    return "See Other";
                case StatusCode::NotModified:
                    return "Not Modified";
                case StatusCode::TemporaryRedirect:
                    return "Temporary Redirect";
                case StatusCode::BadRequest:
                    return "Bad Request";
                case StatusCode::Unauthorized:
                    return "Unauthorized";
                case StatusCode::Forbidden:
                    return "Forbidden";
                case StatusCode::NotFound:
                    return "Not Found";
                case StatusCode::MethodNotAllowed:
                    return "Method Not Allowed";
                case StatusCode::Conflict:
                    return "Conflict";
                case StatusCode::TooManyRequests:
                    return "Too Many Requests";
                case StatusCode::InternalServerError:
                    return "Internal Server Error";
                case StatusCode::NotImplemented:
                    return "Not Implemented";
                case StatusCode::BadGateway:
                    return "Bad Gateway";
                case StatusCode::ServiceUnavailable:
                    return "Service Unavailable";
                default:
                    return "OK";
            }
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
