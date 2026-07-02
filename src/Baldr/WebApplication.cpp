#include "Baldr/WebApplication.hpp"

#include <filesystem>
#include <fstream>
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
}

namespace Baldr::Detail
{
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

        return { StatusCode::OK, fileToServe, detectMimeType(fileToServe),
                 ss.str() };
    }
}

void WebApplication::MapStaticFiles(const std::string& urlPrefix,
                                    const std::string& rootPath)
{
    const std::string prefix = urlPrefix;
    const std::string root   = rootPath;

    MapGet(prefix + "/**", [prefix, root](HttpRequest& request,
                                          HttpResponse&) -> ContentResult {
        (void)prefix;
        auto it = request.params.find("filepath");
        std::string filepath =
            (it != request.params.end()) ? it->second : std::string {};

        auto resolved = Baldr::Detail::resolveStaticFile(filepath, root);
        if (resolved.status == StatusCode::OK)
            return ContentResult(resolved.body, resolved.mimeType,
                                 StatusCode::OK);
        return ContentResult("", "text/plain", resolved.status);
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