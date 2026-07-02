#include "Baldr/WebApplication.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "Baldr/HttpServer.hpp"
#include "Baldr/Result.hpp"

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

    bool isPathSafe(const std::string& path)
    {
        if (path.empty() || path[0] == '/')
            return false;
        if (path.find("..") != std::string::npos)
            return false;
        return true;
    }
}

void WebApplication::MapStaticFiles(const std::string& urlPrefix,
                                    const std::string& rootPath)
{
    const std::string prefix = urlPrefix;
    const std::string root   = rootPath;

    MapGet(prefix + "/:filepath", [prefix, root](HttpRequest& request,
                                                  HttpResponse&)
                                        -> ContentResult {
               auto it = request.params.find("filepath");
               if (it == request.params.end())
                   return ContentResult("", "text/plain",
                                        StatusCode::NotFound);
               const auto& filepath = it->second;

               if (!isPathSafe(filepath))
                   return ContentResult("", "text/plain",
                                        StatusCode::BadRequest);

               std::filesystem::path filePath =
                   std::filesystem::path(root) / filepath;
               std::error_code       ec;
               const auto            canonical =
                   std::filesystem::weakly_canonical(filePath, ec);
               if (ec)
                   return ContentResult("", "text/plain",
                                        StatusCode::NotFound);

               const auto rootCanonical =
                   std::filesystem::weakly_canonical(root, ec);
               if (ec ||
                   canonical.string().find(rootCanonical.string()) != 0)
                   return ContentResult("", "text/plain",
                                        StatusCode::Forbidden);

               if (!std::filesystem::is_regular_file(canonical, ec))
                   return ContentResult("", "text/plain",
                                        StatusCode::NotFound);

               std::ifstream file(canonical, std::ios::binary);
               if (!file)
                   return ContentResult("", "text/plain",
                                        StatusCode::InternalServerError);

               std::ostringstream ss;
               ss << file.rdbuf();

               return ContentResult(ss.str(), detectMimeType(canonical));
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