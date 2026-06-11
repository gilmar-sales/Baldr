#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include <Skirnir/Common/Namespace.hpp>
#include <h2o.h>
#include <picohttpparser.h>

#include "Baldr/HttpMethod.hpp"
#include "Baldr/MiddlewareProvider.hpp"
#include "Baldr/StatusCode.hpp"
#include "Baldr/StringHelpers.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"

class HttpConnection
{
  public:
    struct H2oHandler
    {
        h2o_handler_t super;
        HttpConnection* connection;
    };

    HttpConnection(const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mServiceProvider(serviceProvider),
        mMiddlewareProvider(serviceProvider->GetService<MiddlewareProvider>()),
        mRouter(serviceProvider->GetService<Router>()),
        mLogger(serviceProvider->GetService<skr::Logger<HttpConnection>>())
    {
    }

    static int onRequest(h2o_handler_t* self, h2o_req_t* req)
    {
        auto* handler = reinterpret_cast<H2oHandler*>(self);
        return handler->connection->handle(req);
    }

  private:
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

    static std::string versionString(int version)
    {
        int major = version >> 8;
        int minor = version & 0xff;
        return "HTTP/" + std::to_string(major) + "." + std::to_string(minor);
    }

    static std::string peerIp(h2o_req_t* req)
    {
        struct sockaddr_storage sa {};
        socklen_t               salen = 0;
        if (req->conn->callbacks->get_peername != nullptr)
        {
            salen =
                req->conn->callbacks->get_peername(req->conn, (struct sockaddr*)&sa);
        }
        if (salen == 0)
            return "";
        char buf[NI_MAXHOST] = { 0 };
        if (getnameinfo((struct sockaddr*)&sa, salen, buf, sizeof(buf), nullptr,
                        0, NI_NUMERICHOST) != 0)
        {
            return "";
        }
        return std::string(buf);
    }

    int handle(h2o_req_t* req)
    {
        mLogger->LogDebug("onRequest method={} path={}", std::string(req->method.base, req->method.len), std::string(req->path.base, req->path.len));
        try
        {
        HttpRequest request;
        request.method    = parseMethod(
            std::string_view(req->method.base, req->method.len));
        request.version   = versionString(req->version);

        std::string_view fullPath(req->path.base, req->path.len);
        std::string_view pathOnly = fullPath;
        if (req->query_at != SIZE_MAX && req->query_at <= fullPath.size())
        {
            pathOnly = fullPath.substr(0, req->query_at);
            std::string_view query(
                fullPath.data() + req->query_at + 1,
                fullPath.size() - req->query_at - 1);
            parseQuery(query, request);
        }
        if (auto decoded = decode_path(std::string(pathOnly));
            decoded.has_value())
        {
            request.path = std::move(*decoded);
        }
        else
        {
            h2o_send_error_400(req, "Bad Request", "Invalid URL encoding", 0);
            return 0;
        }

        for (size_t i = 0; i < req->headers.size; ++i)
        {
            const auto& h = req->headers.entries[i];
            std::string name = toLowerAscii(
                std::string_view(h.name->base, h.name->len));
            if (name == "host")
                continue;
            std::string value(h.value.base, h.value.len);
            if (!request.headers.contains(name))
                request.headers.emplace(std::move(name), std::move(value));
        }
        if (req->entity.base != nullptr && req->entity.len > 0)
        {
            request.body.assign(req->entity.base, req->entity.len);
        }
        request.clientIp = peerIp(req);

        auto httpResponse = HttpResponse(request);

        const auto& routeEntry =
            mRouter->match(request.method, request.path);
        if (!routeEntry.has_value())
        {
            mLogger->LogWarning("no route for {} {}", refl::enum_to_string(request.method), request.path);
            httpResponse.statusCode = StatusCode::NotFound;
            httpResponse.body = "Not Found";
            httpResponse.headers["Content-Type"] = "plain/text";
            httpResponse.headers["Content-Length"] = std::to_string(httpResponse.body.size());
            sendResponse(req, httpResponse, /*closeConnection=*/false);
            return 0;
        }

        request.params =
            routeEntry.value().extractRouteParams(request.path);

        auto           scope = mServiceProvider->CreateServiceScope();
        auto           scopedProvider = scope->GetServiceProvider();
        auto           current = mMiddlewareProvider->begin();

        std::function<void()> next;
        next = [&]() {
            auto nextIt = current + 1;
            if (nextIt != mMiddlewareProvider->end())
            {
                ++current;
                (*nextIt)(scopedProvider)
                    ->Handle(request, httpResponse, next);
            }
            else
            {
                routeEntry.value().handler(request, httpResponse,
                                            scopedProvider);
            }
        };

        (*current)(scopedProvider)
            ->Handle(request, httpResponse, next);

        if (httpResponse.body.empty() &&
            httpResponse.headers.find("Content-Length") == httpResponse.headers.end())
        {
            httpResponse.headers["Content-Length"] = "0";
        }
        if (httpResponse.headers.find("Content-Length") == httpResponse.headers.end())
        {
            httpResponse.headers["Content-Length"] =
                std::to_string(httpResponse.body.size());
        }

        bool closeConnection = false;
        if (auto it = httpResponse.headers.find("Connection");
            it != httpResponse.headers.end())
        {
            std::string lowered(it->second);
            for (auto& c : lowered)
            {
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c + 32);
            }
            if (lowered == "close")
                closeConnection = true;
        }
        sendResponse(req, httpResponse, closeConnection);
        }
        catch (const std::exception& e)
        {
            mLogger->LogError("handler exception: {}", e.what());
            h2o_send_error_500(req, "Internal Server Error", e.what(), 0);
        }
        catch (...)
        {
            mLogger->LogError("handler unknown exception");
            h2o_send_error_500(req, "Internal Server Error", "internal error", 0);
        }
        return 0;
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

    static void sendResponse(h2o_req_t* req, const HttpResponse& response,
                             bool closeConnection)
    {
        req->res.status = static_cast<int>(response.statusCode);
        req->res.reason = reasonPhrase(response.statusCode);
        req->res.content_length = response.body.size();

        for (const auto& [name, value] : response.headers)
        {
            if (name == "Connection" || name == "Content-Length")
                continue;
            h2o_add_header_by_str(&req->pool, &req->res.headers, name.data(),
                                  name.size(), 0, NULL, value.data(),
                                  value.size());
        }

        for (const auto& [cookieName, cookieOptions] : response.cookies)
        {
            std::string cookie = cookieName + "=" + cookieOptions.value;
            switch (cookieOptions.sameSite)
            {
                case SameSite::None:
                    cookie += "; SameSite=None";
                    break;
                case SameSite::Lax:
                    cookie += "; SameSite=Lax";
                    break;
                case SameSite::Strict:
                    cookie += "; SameSite=Strict";
                    break;
            }
            if (cookieOptions.domain.has_value())
                cookie += "; Domain=" + cookieOptions.domain.value();
            if (cookieOptions.secure)
                cookie += "; Secure";
            if (cookieOptions.httpOnly)
                cookie += "; HttpOnly";
            if (cookieOptions.maxAge)
                cookie += "; Max-Age=" + std::to_string(cookieOptions.maxAge);

            h2o_add_header_by_str(&req->pool, &req->res.headers,
                                  H2O_TOKEN_SET_COOKIE->buf.base,
                                  H2O_TOKEN_SET_COOKIE->buf.len, 0, NULL,
                                  cookie.data(), cookie.size());
        }

        if (closeConnection)
        {
            h2o_add_header_by_str(&req->pool, &req->res.headers,
                                  H2O_TOKEN_CONNECTION->buf.base,
                                  H2O_TOKEN_CONNECTION->buf.len, 0, NULL,
                                  H2O_STRLIT("close"));
        }

        if (response.body.empty())
        {
            h2o_send_inline(req, "", 0);
        }
        else
        {
            h2o_send_inline(req, response.body.data(), response.body.size());
        }
    }

    static const char* reasonPhrase(StatusCode status)
    {
        switch (status)
        {
            case StatusCode::OK: return "OK";
            case StatusCode::Created: return "Created";
            case StatusCode::Accepted: return "Accepted";
            case StatusCode::NoContent: return "No Content";
            case StatusCode::MovedPermanently: return "Moved Permanently";
            case StatusCode::Found: return "Found";
            case StatusCode::SeeOther: return "See Other";
            case StatusCode::NotModified: return "Not Modified";
            case StatusCode::TemporaryRedirect: return "Temporary Redirect";
            case StatusCode::BadRequest: return "Bad Request";
            case StatusCode::Unauthorized: return "Unauthorized";
            case StatusCode::Forbidden: return "Forbidden";
            case StatusCode::NotFound: return "Not Found";
            case StatusCode::MethodNotAllowed: return "Method Not Allowed";
            case StatusCode::Conflict: return "Conflict";
            case StatusCode::TooManyRequests: return "Too Many Requests";
            case StatusCode::InternalServerError: return "Internal Server Error";
            case StatusCode::NotImplemented: return "Not Implemented";
            case StatusCode::BadGateway: return "Bad Gateway";
            case StatusCode::ServiceUnavailable: return "Service Unavailable";
            default: return "OK";
        }
    }

    skr::Arc<skr::ServiceProvider>        mServiceProvider;
    skr::Arc<MiddlewareProvider>          mMiddlewareProvider;
    skr::Arc<Router>                      mRouter;
    skr::Arc<skr::Logger<HttpConnection>> mLogger;
};
