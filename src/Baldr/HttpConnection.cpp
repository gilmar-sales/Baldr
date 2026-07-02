#include "HttpConnection.hpp"

#include <functional>
#include <utility>

#include <trantor/net/TcpConnection.h>

#include "Baldr/StreamingResult.hpp"

void HttpConnection::runMiddlewareChain(
    MiddlewareFactoryList&                  factories,
    const skr::Arc<skr::ServiceProvider>&   scopedProvider,
    HttpRequest&                            request,
    HttpResponse&                           response,
    const RouteHandler&                     finalHandler)
{
    const size_t factoryCount = factories.size();
    size_t       index       = 0;

    std::function<void()> runRest;

    runRest = [&]() {
        if (index < factoryCount)
        {
            auto factory = factories.begin() + index;
            ++index;
            (*factory)(scopedProvider)->Handle(request, response, runRest);
        }
        else
        {
            finalHandler(request, response, scopedProvider);
        }
    };

    runRest();
}

void HttpConnection::onMessage(trantor::MsgBuffer* buffer)
{
    if (mAccumulator.size() + buffer->readableBytes() > kMaxAccumulatorBytes)
    {
        mLogger->LogError(
            "accumulator overflow ({} bytes); closing connection",
            mAccumulator.size() + buffer->readableBytes());
        buffer->retrieveAll();
        sendErrorResponse(StatusCode::BadRequest,
                          "Request too large");
        if (mConnection && mConnection->connected())
            mConnection->forceClose();
        mAccumulator.clear();
        return;
    }

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
    {
        if (mAccumulator.size() < 4096)
            mAccumulator.erase(0, status.consumedBytes);
        else
            mAccumulator = mAccumulator.substr(status.consumedBytes);
    }
    else
    {
        mAccumulator.clear();
    }

    handle(std::move(request));
}

void HttpConnection::handle(HttpRequest request)
{
    mLogger->LogDebug("onRequest method={} path={}",
                      refl::enum_to_string(request.method), request.path);
    ++mRequestCount;
    if (mInFlightTracker)
        mInFlightTracker->enter();

    struct LeaveGuard
    {
        skr::Arc<InFlightTracker>& t;
        ~LeaveGuard()
        {
            if (t)
                t->leave();
        }
    } _guard { mInFlightTracker };

    try
    {
        HttpResponse httpResponse(request);

        const auto matchResult =
            mRouter->matchWithAllow(request.method, request.path);

        if (!matchResult.entry.has_value())
        {
            if (!matchResult.allowedMethodsOnPath.empty())
            {
                std::string allowList;
                for (auto m : matchResult.allowedMethodsOnPath)
                {
                    if (!allowList.empty())
                        allowList += ", ";
                    allowList += refl::enum_to_string(m);
                }
                mLogger->LogWarning("method not allowed: {} {} (Allow: {})",
                                    refl::enum_to_string(request.method),
                                    request.path, allowList);
                httpResponse.statusCode         = StatusCode::MethodNotAllowed;
                httpResponse.body               = "Method Not Allowed";
                httpResponse.headers["Content-Type"]   = "text/plain";
                httpResponse.headers["Content-Length"] =
                    std::to_string(httpResponse.body.size());
                httpResponse.headers["Allow"]    = allowList;
                sendResponse(httpResponse, /*closeConnection=*/true);
                return;
            }

            mLogger->LogWarning("no route for {} {}",
                                refl::enum_to_string(request.method),
                                request.path);
            httpResponse.statusCode         = StatusCode::NotFound;
            httpResponse.body               = "Not Found";
            httpResponse.headers["Content-Type"]   = "plain/text";
            httpResponse.headers["Content-Length"] =
                std::to_string(httpResponse.body.size());
            sendResponse(httpResponse, /*closeConnection=*/true);
            return;
        }

        request.params =
            matchResult.entry.value().extractRouteParams(request.path);

        auto scope          = mServiceProvider->CreateServiceScope();
        auto scopedProvider = scope->GetServiceProvider();

        auto factories = mMiddlewareProvider->Factories();
        runMiddlewareChain(factories, scopedProvider, request, httpResponse,
                           matchResult.entry.value().handler);

        if (httpResponse.streaming)
        {
            sendStreamingResponse(*httpResponse.streaming,
                                  httpResponse.version,
                                  httpResponse.cookies);
            return;
        }

        // HEAD responses must not include a body, even if the handler
        // wrote one (we transparently routed to the GET handler).
        if (request.method == HttpMethod::Head)
        {
            httpResponse.body.clear();
        }

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

        // Determine connection lifetime.
        //
        // 1. Handler may have explicitly set Connection: close.
        // 2. Peer-supplied `Connection: close` forces close.
        // 3. HTTP/1.0 defaults to close.
        // 4. Server policy may disable keep-alive globally.
        // 5. Per-connection request cap forces close when reached.
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
        if (auto it = request.headers.find("connection");
            it != request.headers.end())
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
        if (request.version == "HTTP/1.0")
            closeConnection = true;
        if (!mServerOptions->enableHttp11KeepAlive)
            closeConnection = true;
        if (mServerOptions->maxRequestsPerConnection > 0 &&
            mRequestCount >= mServerOptions->maxRequestsPerConnection)
        {
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
    size_t      headersBytes = 0;
    for (const auto& [name, value] : response.headers)
    {
        if (name == "Connection")
            continue;
        headersBytes += name.size() + 2 + value.size() + 2;
    }
    out.reserve(response.body.size() + headersBytes + 128);
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
        out.append(name.data(), name.size());
        out.append(": ");
        out.append(value.data(), value.size());
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
        out.append(cookie.data(), cookie.size());
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
    out.append(response.body.data(), response.body.size());

    mConnection->send(std::move(out));

    if (closeConnection)
        mConnection->forceClose();
}

void HttpConnection::sendStreamingResponse(
    const IStreamingResult& result,
    const std::string&      version,
    const std::unordered_map<std::string, CookieOptions>& cookies)
{
    if (!mConnection || !mConnection->connected())
        return;

    std::vector<std::pair<std::string, std::string>> headers;
    result.headers(headers);
    headers.emplace_back("Content-Type", "application/octet-stream");

    std::vector<std::pair<std::string, std::string>> cookieStrings;
    cookieStrings.reserve(cookies.size());
    for (const auto& [cookieName, cookieOptions] : cookies)
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
        cookieStrings.emplace_back(cookieName, std::move(cookie));
    }

    std::string out = formatStreamingHead(
        result.statusCode(), version.empty() ? "HTTP/1.1" : version,
        headers, cookieStrings, &HttpConnection::reasonPhrase);

    mConnection->send(std::move(out));

    std::string chunk;
    while (result.nextChunk(chunk))
    {
        mConnection->send(formatChunk(chunk));
    }
    mConnection->send(formatChunkTrailer());
}
