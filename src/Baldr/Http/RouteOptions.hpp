#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <Baldr/Http/Method.hpp>

namespace Baldr
{
    struct RouteOptions
    {
        std::optional<std::string>         summary;
        std::optional<std::string>         description;
        std::vector<std::string>           tags;
        std::optional<std::string>         operationId;
        bool                               deprecated = false;
        std::vector<std::string>           consumes;
        std::vector<std::string>           produces;
        std::unordered_map<std::string, std::string> metadata;
    };

    struct RouteInfo
    {
        std::string path;
        std::string group;
        HttpMethod  method { HttpMethod::Get };
        RouteOptions options;
    };
}