#pragma once

#include <optional>
#include <string>

enum class SameSite {
    None,
    Lax,
    Strict
};

struct CookieOptions {
    std::string value;
    std::optional<std::string> domain;
    std::optional<SameSite> sameSite;
    long maxAge{};
    bool httpOnly{};
    bool secure{};
};