#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>

namespace BALDR_NAMESPACE {

enum class SameSite
{
    None,
    Lax,
    Strict
};

struct CookieOptions
{
    std::string                value;
    std::optional<std::string> domain;
    SameSite                   sameSite = SameSite::None;
    long                       maxAge {};
    bool                       httpOnly {};
    bool                       secure {};
};

} // namespace BALDR_NAMESPACE