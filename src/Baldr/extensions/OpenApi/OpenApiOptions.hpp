#pragma once

#include <optional>
#include <string>

namespace Baldr::OpenApi
{
    struct Info
    {
        std::string              title       = "Baldr API";
        std::string              version     = "0.15.1";
        std::optional<std::string> description;
    };

    struct OpenApiOptions
    {
        std::string mountPath = "/openapi.json";
        Info         info;
        bool         enabled = true;
    };
}