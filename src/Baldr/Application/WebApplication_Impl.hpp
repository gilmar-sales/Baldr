/**
 * @file Application/WebApplication_Impl.hpp
 * @brief PIMPL holder for @c WebApplication; kept private to the library.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <memory>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/Router.hpp>
#include <Baldr/Middleware/MiddlewareProvider.hpp>

namespace BALDR_NAMESPACE
{

    namespace detail
    {
        /**
         * @brief Internal state shared by @c WebApplication methods.
         *
         * Held via @c std::unique_ptr from the public header so that the
         * router and middleware headers don't leak into every translation
         * unit that includes @c WebApplication.hpp.
         */
        struct WebApplicationImpl
        {
            skr::Arc<Router>             mRouter;
            skr::Arc<MiddlewareProvider> mMiddlewareProvider;
        };
    } // namespace detail

} // namespace BALDR_NAMESPACE