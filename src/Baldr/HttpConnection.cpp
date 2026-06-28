#include "HttpConnection.hpp"

#include <functional>
#include <utility>

#include <trantor/net/TcpConnection.h>

void HttpConnection::onMessage(trantor::MsgBuffer* buffer)
{
    mAccumulator.append(buffer->peek(), buffer->readableBytes());
    buffer->retrieveAll();

    auto status = mParser->tryParse(mAccumulator);
    if (status.kind == HttpParseStatus::Kind::Incomplete)
    {
        return;
    }

    if (status.kind == HttpParseStatus::Kind::Error)
    {
        mLogger->LogWarning("parse error: {}", status.errorMessage);
        sendErrorResponse(status.statusCode, status.errorMessage);
        mAccumulator.clear();
        return;
    }

    HttpRequest request = std::move(status.request);
    request.clientIp    = mClientIp;
    if (mAccumulator.size() >= status.consumedBytes)
        mAccumulator.erase(0, status.consumedBytes);
    else
        mAccumulator.clear();

    handle(std::move(request));
}

void HttpConnection::handle(HttpRequest request)
{
    mLogger->LogDebug("onRequest method={} path={}",
                      refl::enum_to_string(request.method), request.path);
    try
    {
        HttpResponse httpResponse(request);

        const auto& routeEntry =
            mRouter->match(request.method, request.path);
        if (!routeEntry.has_value())
        {
            mLogger->LogWarning("no route for {} {}",
                                refl::enum_to_string(request.method),
                                request.path);
            httpResponse.statusCode = StatusCode::NotFound;
            httpResponse.body = "Not Found";
            httpResponse.headers["Content-Type"]   = "plain/text";
            httpResponse.headers["Content-Length"] =
                std::to_string(httpResponse.body.size());
            sendResponse(httpResponse, /*closeConnection=*/true);
            return;
        }

        request.params =
            routeEntry.value().extractRouteParams(request.path);

        auto scope          = mServiceProvider->CreateServiceScope();
        auto scopedProvider = scope->GetServiceProvider();
        auto current        = mMiddlewareProvider->begin();

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

        (*current)(scopedProvider)->Handle(request, httpResponse, next);

        if (httpResponse.body.empty() &&
            httpResponse.headers.find("Content-Length") ==
                httpResponse.headers.end())
        {
            httpResponse.headers["Content-Length"] = "0";
        }
        if (httpResponse.headers.find("Content-Length") ==
            httpResponse.headers.end())
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

        sendResponse(httpResponse, closeConnection);
    }
    catch (const std::exception& e)
    {
        mLogger->LogError("handler exception: {}", e.what());
        sendErrorResponse(StatusCode::InternalServerError, e.what());
    }
    catch (...)
    {
        mLogger->LogError("handler unknown exception");
        sendErrorResponse(StatusCode::InternalServerError, "internal error");
    }
}

void HttpConnection::sendErrorResponse(StatusCode      statusCode,
                                       const std::string& body)
{
    HttpResponse response;
    response.version          = "HTTP/1.1";
    response.statusCode       = statusCode;
    response.body             = body;
    response.headers["Content-Type"]   = "plain/text";
    response.headers["Content-Length"] = std::to_string(body.size());
    response.headers["Connection"]     = "close";
    sendResponse(response, /*closeConnection=*/true);
}

void HttpConnection::sendResponse(const HttpResponse& response,
                                  bool                closeConnection)
{
    if (!mConnection || !mConnection->connected())
        return;

    std::string out;
    out.reserve(128 + response.body.size());
    out.append("HTTP/1.1 ");
    out.append(std::to_string(static_cast<int>(response.statusCode)));
    out.push_back(' ');
    out.append(reasonPhrase(response.statusCode));
    out.append("\r\n");

    bool contentLengthEmitted = false;
    for (const auto& [name, value] : response.headers)
    {
        if (name == "Connection")
            continue;
        if (name == "Content-Length")
        {
            contentLengthEmitted = true;
        }
        out.append(name);
        out.append(": ");
        out.append(value);
        out.append("\r\n");
    }

    for (const auto& [cookieName, cookieOptions] : response.cookies)
    {
        std::string cookie = cookieName + "=" + cookieOptions.value;
        switch (cookieOptions.sameSite)
        {
            case SameSite::None:   cookie += "; SameSite=None";   break;
            case SameSite::Lax:    cookie += "; SameSite=Lax";    break;
            case SameSite::Strict: cookie += "; SameSite=Strict"; break;
        }
        if (cookieOptions.domain.has_value())
            cookie += "; Domain=" + cookieOptions.domain.value();
        if (cookieOptions.secure)  cookie += "; Secure";
        if (cookieOptions.httpOnly) cookie += "; HttpOnly";
        if (cookieOptions.maxAge)
            cookie += "; Max-Age=" + std::to_string(cookieOptions.maxAge);
        out.append("Set-Cookie: ");
        out.append(cookie);
        out.append("\r\n");
    }

    if (closeConnection)
        out.append("Connection: close\r\n");

    if (!contentLengthEmitted)
    {
        out.append("Content-Length: ");
        out.append(std::to_string(response.body.size()));
        out.append("\r\n");
    }

    out.append("\r\n");
    out.append(response.body);

    mConnection->send(std::move(out));

    if (closeConnection)
        mConnection->forceClose();
}
