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
        struct WebApplicationImpl
        {
            skr::Arc<Router>             mRouter;
            skr::Arc<MiddlewareProvider> mMiddlewareProvider;
        };
    } // namespace detail

} // namespace BALDR_NAMESPACE