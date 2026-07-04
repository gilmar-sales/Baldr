#pragma once

#include <memory>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Http/Router.hpp>
#include <Baldr/Middleware/MiddlewareProvider.hpp>

namespace Baldr::detail
{
    struct WebApplicationImpl
    {
        skr::Arc<Router>             mRouter;
        skr::Arc<MiddlewareProvider> mMiddlewareProvider;
    };
} // namespace Baldr::detail