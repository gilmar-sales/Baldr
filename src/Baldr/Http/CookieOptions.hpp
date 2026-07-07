/**
 * @file Http/CookieOptions.hpp
 * @brief Cookie attributes used by @c HttpResponse::cookies and by the
 *        CSRF middleware when issuing a token cookie.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <optional>
#include <string>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Values for the @c SameSite cookie attribute.
     */
    enum class SameSite
    {
        None,  ///< @c SameSite=None (cross-site allowed; requires @c Secure)
        Lax,   ///< @c SameSite=Lax (top-level navigations allowed)
        Strict ///< @c SameSite=Strict (no cross-site requests)
    };

    /**
     * @brief Serialised cookie attributes consumed by the response writer.
     *
     * The framework turns each entry in @c HttpResponse::cookies into a
     * @c Set-Cookie header; @c value is the only required attribute.
     */
    struct CookieOptions
    {
        std::string                value;  ///< Cookie value sent to the client.
        std::optional<std::string> domain; ///< Optional @c Domain attribute.
        SameSite sameSite = SameSite::None; ///< @c SameSite attribute.
        long     maxAge {};   ///< Optional @c Max-Age, in seconds. 0 = session.
        bool     httpOnly {}; ///< When true, sets @c HttpOnly.
        bool     secure {};   ///< When true, sets @c Secure.
    };

} // namespace BALDR_NAMESPACE