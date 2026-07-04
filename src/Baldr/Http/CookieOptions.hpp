#pragma once

#include <optional>
#include <string>

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
    SameSite    sameSite = SameSite::None;
    long                       maxAge {};
    bool                       httpOnly {};
    bool                       secure {};
};