#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>

namespace BALDR_NAMESPACE {

struct Info
{
    std::string                title   = "Baldr API";
    std::string                version = "0.15.1";
    std::optional<std::string> description;
};

struct OpenApiOptions
{
    std::string mountPath = "/openapi.json";
    Info        info;
    bool        enabled = true;
};

} // namespace BALDR_NAMESPACE