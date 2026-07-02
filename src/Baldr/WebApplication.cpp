#include "Baldr/WebApplication.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Baldr/HttpServer.hpp"
#include "Baldr/Result.hpp"
#include "StaticFilesInternal.hpp"

namespace
{
    const std::unordered_map<std::string, std::string>& mimeTypes()
    {
        static const std::unordered_map<std::string, std::string> table = {
            { ".html", "text/html" },     { ".htm", "text/html" },
            { ".css",  "text/css" },      { ".js",  "application/javascript" },
            { ".json", "application/json" }, { ".svg", "image/svg+xml" },
            { ".png",  "image/png" },     { ".jpg",  "image/jpeg" },
            { ".jpeg", "image/jpeg" },    { ".gif",  "image/gif" },
            { ".webp", "image/webp" },    { ".ico",  "image/x-icon" },
            { ".txt",  "text/plain" },    { ".pdf",  "application/pdf" },
            { ".woff", "font/woff" },     { ".woff2", "font/woff2" },
        };
        return table;
    }

    std::string detectMimeType(const std::filesystem::path& p)
    {
        auto ext = p.extension().string();
        for (auto& c : ext)
            c = static_cast<char>(std::tolower(c));
        auto it = mimeTypes().find(ext);
        if (it != mimeTypes().end())
            return it->second;
        return "application/octet-stream";
    }

    std::vector<std::string> splitSegments(const std::string& s)
    {
        std::vector<std::string> out;
        std::string              cur;
        for (char c : s)
        {
            if (c == '/')
            {
                if (!cur.empty())
                {
                    out.push_back(cur);
                    cur.clear();
                }
            }
            else
            {
                cur.push_back(c);
            }
        }
        if (!cur.empty())
            out.push_back(cur);
        return out;
    }

    std::string toLowerAscii(std::string_view s)
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
}

namespace Baldr::Detail
{
    std::string makeEtag(std::uintmax_t              size,
                         std::chrono::system_clock::time_point mtime)
    {
        std::ostringstream oss;
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                        mtime.time_since_epoch())
                        .count();
        oss << "\"" << std::hex << size << '-' << secs << std::dec << "\"";
        return oss.str();
    }

    std::string formatHttpDate(std::chrono::system_clock::time_point tp)
    {
        std::time_t  tt = std::chrono::system_clock::to_time_t(tp);
        std::tm      tm {};
#if defined(_WIN32)
        gmtime_s(&tm, &tt);
#else
        gmtime_r(&tt, &tm);
#endif
        char buf[64];
        // IMF-fixdate: Sun, 06 Nov 1994 08:49:37 GMT
        std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        return std::string(buf);
    }

    std::chrono::system_clock::time_point parseHttpDate(std::string_view v)
    {
        // IMF-fixdate example: "Sun, 06 Nov 1994 08:49:37 GMT"
        // RFC 850:            "Sunday, 06-Nov-94 08:49:37 GMT"
        // asctime:            "Sun Nov  6 08:49:37 1994"
        // We support all three per RFC 7231.
        if (v.empty())
            return {};

        std::tm tm {};
        std::istringstream iss { std::string(v) };
        iss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        if (iss.fail())
        {
            std::istringstream iss2 { std::string(v) };
            iss2 >> std::get_time(&tm, "%A, %d-%b-%y %H:%M:%S GMT");
            if (iss2.fail())
            {
                std::istringstream iss3 { std::string(v) };
                iss3 >> std::get_time(&tm, "%a %b %d %H:%M:%S %Y");
                if (iss3.fail())
                    return {};
            }
        }

#if defined(_WIN32)
        std::time_t tt = _mkgmtime(&tm);
#else
        std::time_t tt = timegm(&tm);
#endif
        if (tt == static_cast<std::time_t>(-1))
            return {};
        return std::chrono::system_clock::from_time_t(tt);
    }

    StaticResolve resolveStaticFile(const std::string& filepath,
                                    const std::string& root)
    {
        if (filepath.find('\0') != std::string::npos ||
            filepath.find('\\') != std::string::npos)
        {
            return { StatusCode::BadRequest, {}, {}, {} };
        }

        for (const auto& seg : splitSegments(filepath))
        {
            if (seg == ".." || seg == ".")
                return { StatusCode::BadRequest, {}, {}, {} };
        }

        std::error_code ec;
        const auto      rootCanonical =
            std::filesystem::weakly_canonical(root, ec);
        if (ec)
            return { StatusCode::InternalServerError, {}, {}, {} };

        std::filesystem::path requested =
            std::filesystem::path(root) / filepath;
        const auto            canonical =
            std::filesystem::weakly_canonical(requested, ec);
        if (ec)
            return { StatusCode::NotFound, {}, {}, {} };

        const std::string rootStr = rootCanonical.string();
        const std::string fileStr = canonical.string();
        const bool        exactRoot = (fileStr == rootStr);
        const bool        underRoot =
            fileStr.size() > rootStr.size() &&
            fileStr.compare(0, rootStr.size(), rootStr) == 0 &&
            (fileStr[rootStr.size()] == '/' ||
             fileStr[rootStr.size()] == std::filesystem::path::preferred_separator);

        if (!exactRoot && !underRoot)
            return { StatusCode::Forbidden, {}, {}, {} };

        std::filesystem::path fileToServe = canonical;

        std::error_code isDirEc;
        if (std::filesystem::is_directory(canonical, isDirEc))
        {
            fileToServe = canonical / "index.html";
        }

        if (!std::filesystem::is_regular_file(fileToServe, ec))
            return { StatusCode::NotFound, {}, {}, {} };

        std::ifstream file(fileToServe, std::ios::binary);
        if (!file)
            return { StatusCode::InternalServerError, {}, {}, {} };

        std::ostringstream ss;
        ss << file.rdbuf();

        std::error_code            sizec;
        auto                      sz  = std::filesystem::file_size(fileToServe, sizec);
        auto                      mte = std::filesystem::last_write_time(fileToServe);
        // Round-trip to system_clock resolution (mtime is filesystem_clock).
        auto                      mtc = std::chrono::file_clock::to_sys(mte);

        return { StatusCode::OK, fileToServe, detectMimeType(fileToServe),
                 ss.str(),
                 /*fileSize=*/sizec ? 0 : sz,
                 /*lastModified=*/mtc,
                 /*etag=*/Baldr::Detail::makeEtag(sizec ? 0 : sz, mtc) };
    }
}

void WebApplication::MapStaticFiles(const std::string& urlPrefix,
                                    const std::string& rootPath)
{
    const std::string prefix = urlPrefix;
    const std::string root   = rootPath;

    MapGet(prefix + "/**", [prefix, root](HttpRequest& request,
                                          HttpResponse& response) -> ContentResult {
        (void)prefix;
        (void)response;
        auto it = request.params.find("filepath");
        std::string filepath =
            (it != request.params.end()) ? it->second : std::string {};

        auto resolved = Baldr::Detail::resolveStaticFile(filepath, root);
        if (resolved.status != StatusCode::OK)
            return ContentResult("", "text/plain", resolved.status);

        const std::string lastModifiedHeader =
            Baldr::Detail::formatHttpDate(resolved.lastModified);

        // If-None-Match (strong comparison; weak etags are not used).
        auto inmIt = request.headers.find("if-none-match");
        if (inmIt != request.headers.end())
        {
            std::string                   inm = toLowerAscii(inmIt->second);
            std::string                   tag = toLowerAscii(resolved.etag);
            // Trim whitespace.
            auto trim = [](std::string& s) {
                while (!s.empty() && std::isspace(static_cast<unsigned char>(
                                          s.back())))
                    s.pop_back();
                while (!s.empty() && std::isspace(static_cast<unsigned char>(
                                          s.front())))
                    s.erase(s.begin());
            };
            trim(inm);
            trim(tag);
            if (inm == tag || inm == "*")
            {
                response.headers["ETag"]              = resolved.etag;
                response.headers["Last-Modified"]     = lastModifiedHeader;
                response.statusCode                   = StatusCode::NotModified;
                return ContentResult("", resolved.mimeType,
                                     StatusCode::NotModified);
            }
        }

        // If-Modified-Since: only meaningful for GETs. We treat it as a
        // secondary cache validator: any modification after the supplied
        // timestamp invalidates the cached entry.
        auto imsIt = request.headers.find("if-modified-since");
        if (imsIt != request.headers.end())
        {
            auto ts = Baldr::Detail::parseHttpDate(imsIt->second);
            // Truncate both to seconds before comparing.
            auto floorToSeconds =
                [](std::chrono::system_clock::time_point tp) {
                    return std::chrono::system_clock::time_point {
                        std::chrono::duration_cast<std::chrono::seconds>(
                            tp.time_since_epoch()) };
                };
            if (floorToSeconds(resolved.lastModified) <= floorToSeconds(ts))
            {
                response.headers["ETag"]          = resolved.etag;
                response.headers["Last-Modified"] = lastModifiedHeader;
                response.statusCode               = StatusCode::NotModified;
                return ContentResult("", resolved.mimeType,
                                     StatusCode::NotModified);
            }
        }

        ContentResult body(resolved.body, resolved.mimeType, StatusCode::OK);
        // Hand the cached headers back through a side-channel: we
        // attach them to the ContentResult's headers by tweaking the
        // response here too. MapRoute will apply ContentResult to the
        // response object; the explicit header writes ensure ETag /
        // Last-Modified reach the wire.
        response.headers["ETag"]          = resolved.etag;
        response.headers["Last-Modified"] = lastModifiedHeader;
        return body;
    });
}

void WebApplication::Run()
{
    auto logger =
        mRootServiceProvider->GetService<skr::Logger<WebApplication>>();
    try
    {
        auto server = mRootServiceProvider->GetService<HttpServer>();

        server->Run();
    }
    catch (const std::exception& e)
    {
        logger->LogError("{}", e.what());
    }
}
