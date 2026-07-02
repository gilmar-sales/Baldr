#pragma once

#include <filesystem>
#include <string>

#include "Baldr/StatusCode.hpp"

namespace Baldr::Detail
{
    struct StaticResolve
    {
        StatusCode            status;
        std::filesystem::path canonical;
        std::string           mimeType;
        std::string           body;
    };

    StaticResolve resolveStaticFile(const std::string& filepath,
                                    const std::string& root);
}