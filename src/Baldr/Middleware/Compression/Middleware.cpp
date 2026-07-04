#include <Baldr/Middleware/Compression/Middleware.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

#include <Baldr/Middleware/Compression/Internal.hpp>

void CompressionMiddleware::Handle(HttpRequest&          request,
                                   HttpResponse&         response,
                                   const NextMiddleware& next)
{
    // Skip when streaming (we can't rewrite an already-chunked body).
    if (response.streaming)
    {
        next();
        return;
    }

    // Snapshot pre-handler state so we can decide whether to compress
    // after the handler runs.
    next();

    // No body or already encoded -> skip.
    if (response.body.empty())
        return;

    std::string already;
    if (auto it = response.headers.find("Content-Encoding");
        it != response.headers.end())
        already = it->second;
    if (!already.empty())
        return;

    // Honour a forced identity via explicit Content-Encoding header.
    // Also avoid compressing 204/304 responses (MUST NOT).
    auto status = static_cast<int>(response.statusCode);
    if (status == 204 || status == 304)
        return;

    // Only compress text-like mime types.
    auto ctypeIt = response.headers.find("Content-Type");
    std::string ctype =
        ctypeIt != response.headers.end() ? ctypeIt->second
                                          : std::string("application/octet-stream");
    if (!mimeAllowed(ctype, mOptions.mimeTypePrefixes))
        return;

    // Honour q-values for gzip in Accept-Encoding. We only need to
    // verify gzip is acceptable at all (q > 0); identity is implied.
    auto acceptIt = request.headers.find("accept-encoding");
    if (acceptIt != request.headers.end())
    {
        std::string ae = toLowerAscii(acceptIt->second);
        bool        gzipAccepted = false;
        // Search for ", gzip" or leading "gzip" optionally followed by
        // ";q=" and a non-zero qvalue. We treat any explicit "gzip;q=0"
        // as a hard opt-out.
        std::size_t pos = 0;
        while (pos < ae.size())
        {
            std::size_t comma = ae.find(',', pos);
            if (comma == std::string::npos)
                comma = ae.size();
            std::string_view token = std::string_view(ae).substr(pos,
                                                                  comma - pos);
            // Trim spaces.
            std::size_t a = 0, b = token.size();
            while (a < b && std::isspace(static_cast<unsigned char>(token[a])))
                ++a;
            while (b > a && std::isspace(static_cast<unsigned char>(token[b - 1])))
                --b;
            token = token.substr(a, b - a);

            std::size_t qPos = token.find(';');
            std::string_view name = token.substr(0, qPos);
            std::size_t nl     = name.size();
            while (nl > 0 &&
                   std::isspace(static_cast<unsigned char>(name[nl - 1])))
                --nl;
            name = name.substr(0, nl);

            double q = 1.0;
            if (qPos != std::string_view::npos)
            {
                std::size_t eq = token.find('=', qPos);
                if (eq != std::string_view::npos)
                {
                    try
                    {
                        q = std::stod(std::string(token.substr(eq + 1)));
                    }
                    catch (...)
                    {
                        q = 1.0;
                    }
                }
                else
                {
                    q = 1.0;
                }
            }

            if (name == "gzip" && q > 0.0)
            {
                gzipAccepted = true;
            }
            else if (name == "*" && q > 0.0)
            {
                gzipAccepted = true;
            }
            pos = comma + 1;
        }
        if (!gzipAccepted)
            return;
    }

    // Skip when below the size threshold.
    if (response.body.size() < mOptions.minBodyBytes)
        return;

    std::string compressed;
    if (!Baldr::Detail::gzipCompress(response.body, compressed, mOptions.level))
        return;

    // Don't bother if compression did not yield a benefit.
    if (compressed.size() >= response.body.size())
        return;

    response.body = std::move(compressed);
    response.headers["Content-Encoding"]    = "gzip";
    response.headers["Content-Length"]      = std::to_string(
                                                  response.body.size());
    response.headers["Vary"]                = "Accept-Encoding";
}

std::string CompressionMiddleware::toLowerAscii(std::string_view s)
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

bool CompressionMiddleware::mimeAllowed(
    const std::string&                      ctype,
    const std::unordered_set<std::string>& prefixes)
{
    std::string lc = toLowerAscii(ctype);
    // Strip parameters (e.g., "text/html; charset=utf-8").
    auto semi = lc.find(';');
    if (semi != std::string::npos)
        lc = lc.substr(0, semi);
    while (!lc.empty() &&
           std::isspace(static_cast<unsigned char>(lc.front())))
        lc.erase(lc.begin());
    while (!lc.empty() && std::isspace(static_cast<unsigned char>(lc.back())))
        lc.pop_back();

    for (const auto& prefix : prefixes)
    {
        std::string p = toLowerAscii(prefix);
        // Trailing-slash prefixes match as a literal start ("text/"
        // matches "text/html" but NOT "text/htmlx"). Exact prefixes
        // match the whole token ("application/json" matches
        // "application/json" but NOT "application/jsonp").
        bool match = false;
        if (!p.empty() && p.back() == '/')
        {
            match = lc.size() >= p.size() &&
                    lc.compare(0, p.size(), p) == 0;
        }
        else
        {
            match = lc == p;
        }
        if (match)
            return true;
    }
    return false;
}
