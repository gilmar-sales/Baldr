#include <Baldr/Application/WebApplication.hpp>
#include <Baldr/Detail/Namespace.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
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

#include <Baldr/Application/RouteListing.hpp>
#include <Baldr/Http/Router.hpp>
#include <Baldr/Http/Server.hpp>
#include <Baldr/Http/StaticFilesInternal.hpp>
#include <Baldr/Middleware/MiddlewareProvider.hpp>

namespace BALDR_NAMESPACE
{

    const std::unordered_map<std::string, std::string>& mimeTypes()
    {
        static const std::unordered_map<std::string, std::string> table = {
            { ".html", "text/html" },
            { ".htm", "text/html" },
            { ".css", "text/css" },
            { ".js", "application/javascript" },
            { ".json", "application/json" },
            { ".svg", "image/svg+xml" },
            { ".png", "image/png" },
            { ".jpg", "image/jpeg" },
            { ".jpeg", "image/jpeg" },
            { ".gif", "image/gif" },
            { ".webp", "image/webp" },
            { ".ico", "image/x-icon" },
            { ".txt", "text/plain" },
            { ".pdf", "application/pdf" },
            { ".woff", "font/woff" },
            { ".woff2", "font/woff2" },
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

    namespace Detail
    {
        std::string makeEtag(std::uintmax_t                        size,
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
            std::time_t tt = std::chrono::system_clock::to_time_t(tp);
            std::tm     tm {};
#if defined(_WIN32)
            gmtime_s(&tm, &tt);
#else
            gmtime_r(&tt, &tm);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
            return std::string(buf);
        }

        std::chrono::system_clock::time_point parseHttpDate(std::string_view v)
        {
            if (v.empty())
                return {};

            std::tm            tm {};
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
            const auto canonical =
                std::filesystem::weakly_canonical(requested, ec);
            if (ec)
                return { StatusCode::NotFound, {}, {}, {} };

            const std::string rootStr   = rootCanonical.string();
            const std::string fileStr   = canonical.string();
            const bool        exactRoot = (fileStr == rootStr);
            const bool        underRoot =
                fileStr.size() > rootStr.size() &&
                fileStr.compare(0, rootStr.size(), rootStr) == 0 &&
                (fileStr[rootStr.size()] == '/' ||
                 fileStr[rootStr.size()] ==
                     std::filesystem::path::preferred_separator);

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

            std::error_code sizec;
            auto            sz = std::filesystem::file_size(fileToServe, sizec);
            auto            mte = std::filesystem::last_write_time(fileToServe);
            auto            mtc = std::chrono::file_clock::to_sys(mte);

            return { StatusCode::OK,
                     fileToServe,
                     detectMimeType(fileToServe),
                     ss.str(),
                     /*fileSize=*/sizec ? 0 : sz,
                     /*lastModified=*/mtc,
                     /*etag=*/Detail::makeEtag(sizec ? 0 : sz, mtc) };
        }
    } // namespace Detail

    WebApplication::WebApplication(
        const skr::Arc<skr::ServiceProvider>& rootServiceProvider) :
        IApplication(rootServiceProvider),
        mImpl(std::make_unique<detail::WebApplicationImpl>())
    {
        mImpl->mRouter = rootServiceProvider->GetService<Router>();
        mImpl->mMiddlewareProvider =
            rootServiceProvider->GetService<MiddlewareProvider>();
    }

    RouteRegistration WebApplication::MapGet(const std::string& route)
    {
        return RouteRegistration(*mImpl->mRouter, HttpMethod::Get, route);
    }

    RouteRegistration WebApplication::MapPost(const std::string& route)
    {
        return RouteRegistration(*mImpl->mRouter, HttpMethod::Post, route);
    }

    RouteRegistration WebApplication::MapPut(const std::string& route)
    {
        return RouteRegistration(*mImpl->mRouter, HttpMethod::Put, route);
    }

    RouteRegistration WebApplication::MapDelete(const std::string& route)
    {
        return RouteRegistration(*mImpl->mRouter, HttpMethod::Delete, route);
    }

    RouteRegistration WebApplication::MapPatch(const std::string& route)
    {
        return RouteRegistration(*mImpl->mRouter, HttpMethod::Patch, route);
    }

    void WebApplication::EnableRouteListing(std::string path)
    {
#ifndef NDEBUG
        auto router = mImpl->mRouter;
        MapGet(path, [router](HttpRequest&) -> ContentResult {
            auto body = RouteListingToJson(router->Snapshot());
            return ContentResult(std::move(body), "application/json",
                                 StatusCode::OK);
        });
#else
        (void) path;
#endif
    }

    namespace
    {
        std::string jsonEscape(std::string_view s)
        {
            std::string out;
            out.reserve(s.size() + 2);
            for (char c : s)
            {
                switch (c)
                {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\b':
                        out += "\\b";
                        break;
                    case '\f':
                        out += "\\f";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20)
                        {
                            char buf[8];
                            std::snprintf(buf, sizeof(buf), "\\u%04x",
                                          static_cast<unsigned char>(c));
                            out += buf;
                        }
                        else
                        {
                            out.push_back(c);
                        }
                }
            }
            return out;
        }

        ContentResult buildHealthResponse(
            const std::vector<HealthCheckRegistration>& checks,
            const HttpRequest&                          request)
        {
            std::unordered_map<std::string, bool> results;
            bool                                  healthy = true;

            for (const auto& c : checks)
            {
                bool ok = false;
                try
                {
                    ok = c.check ? c.check(request) : true;
                }
                catch (...)
                {
                    ok = false;
                }
                results.emplace(c.name, ok);
                if (!ok)
                    healthy = false;
            }

            std::string body;
            body.reserve(64 + results.size() * 16);
            body += "{\"status\":\"";
            body += healthy ? "healthy" : "unhealthy";
            body += "\"";

            if (!results.empty())
            {
                body += ",\"checks\":{";
                bool first = true;
                for (const auto& [name, ok] : results)
                {
                    if (!first)
                        body += ',';
                    first = false;
                    body += '"';
                    body += jsonEscape(name);
                    body += "\":";
                    body += ok ? "true" : "false";
                }
                body += '}';
            }
            body += '}';

            const auto status = (checks.empty() || healthy)
                                    ? StatusCode::OK
                                    : StatusCode::ServiceUnavailable;

            return ContentResult(std::move(body), "application/json", status);
        }
    } // namespace

    void WebApplication::MapHealthChecks(std::vector<std::string> paths,
                                         std::vector<HealthCheckRegistration>
                                                     checks,
                                         std::string livePath)
    {
        for (const auto& p : paths)
        {
            MapGet(p, [checks](HttpRequest& request) -> ContentResult {
                return buildHealthResponse(checks, request);
            });
        }

        if (!livePath.empty())
        {
            MapGet(livePath, [](HttpRequest&) -> ContentResult {
                return ContentResult(R"({"status":"healthy"})",
                                     "application/json", StatusCode::OK);
            });
        }
    }

    void WebApplication::MapStaticFiles(const std::string& urlPrefix,
                                        const std::string& rootPath)
    {
        const std::string prefix = urlPrefix;
        const std::string root   = rootPath;

        MapGet(
            prefix + "/**",
            [prefix, root](HttpRequest&  request,
                           HttpResponse& response) -> ContentResult {
                (void) prefix;
                (void) response;
                auto        it = request.params.find("filepath");
                std::string filepath =
                    (it != request.params.end()) ? it->second : std::string {};

                auto resolved = Detail::resolveStaticFile(filepath, root);
                if (resolved.status != StatusCode::OK)
                    return ContentResult("", "text/plain", resolved.status);

                const std::string lastModifiedHeader =
                    Detail::formatHttpDate(resolved.lastModified);

                auto inmIt = request.headers.find("if-none-match");
                if (inmIt != request.headers.end())
                {
                    std::string inm  = toLowerAscii(inmIt->second);
                    std::string tag  = toLowerAscii(resolved.etag);
                    auto        trim = [](std::string& s) {
                        while (
                            !s.empty() &&
                            std::isspace(static_cast<unsigned char>(s.back())))
                            s.pop_back();
                        while (
                            !s.empty() &&
                            std::isspace(static_cast<unsigned char>(s.front())))
                            s.erase(s.begin());
                    };
                    trim(inm);
                    trim(tag);
                    if (inm == tag || inm == "*")
                    {
                        response.headers["ETag"]          = resolved.etag;
                        response.headers["Last-Modified"] = lastModifiedHeader;
                        response.statusCode = StatusCode::NotModified;
                        return ContentResult("", resolved.mimeType,
                                             StatusCode::NotModified);
                    }
                }

                auto imsIt = request.headers.find("if-modified-since");
                if (imsIt != request.headers.end())
                {
                    auto ts = Detail::parseHttpDate(imsIt->second);
                    auto floorToSeconds =
                        [](std::chrono::system_clock::time_point tp) {
                            return std::chrono::system_clock::time_point {
                                std::chrono::duration_cast<
                                    std::chrono::seconds>(tp.time_since_epoch())
                            };
                        };
                    if (floorToSeconds(resolved.lastModified) <=
                        floorToSeconds(ts))
                    {
                        response.headers["ETag"]          = resolved.etag;
                        response.headers["Last-Modified"] = lastModifiedHeader;
                        response.statusCode = StatusCode::NotModified;
                        return ContentResult("", resolved.mimeType,
                                             StatusCode::NotModified);
                    }
                }

                ContentResult body(resolved.body,
                                   resolved.mimeType,
                                   StatusCode::OK);
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

    skr::Arc<Router> WebApplication::GetRouter() const
    {
        return mImpl->mRouter;
    }

    std::string WebApplication::RouteBuilder::join(const std::string& a,
                                                   const std::string& b)
    {
        if (a.empty())
            return b;
        if (b.empty())
            return a;
        if (a.back() == '/' && b.front() == '/')
            return a + b.substr(1);
        if (a.back() == '/' || b.front() == '/')
            return a + b;
        return a + "/" + b;
    }

    RouteRegistration WebApplication::RouteBuilder::MapGet(
        const std::string& route)
    {
        return RouteRegistration(mRouter,
                                 HttpMethod::Get,
                                 join(mPrefix, route),
                                 mPrefix);
    }

    RouteRegistration WebApplication::RouteBuilder::MapPost(
        const std::string& route)
    {
        return RouteRegistration(mRouter,
                                 HttpMethod::Post,
                                 join(mPrefix, route),
                                 mPrefix);
    }

    RouteRegistration WebApplication::RouteBuilder::MapPut(
        const std::string& route)
    {
        return RouteRegistration(mRouter,
                                 HttpMethod::Put,
                                 join(mPrefix, route),
                                 mPrefix);
    }

    RouteRegistration WebApplication::RouteBuilder::MapDelete(
        const std::string& route)
    {
        return RouteRegistration(mRouter,
                                 HttpMethod::Delete,
                                 join(mPrefix, route),
                                 mPrefix);
    }

    RouteRegistration WebApplication::RouteBuilder::MapPatch(
        const std::string& route)
    {
        return RouteRegistration(mRouter,
                                 HttpMethod::Patch,
                                 join(mPrefix, route),
                                 mPrefix);
    }

} // namespace BALDR_NAMESPACE