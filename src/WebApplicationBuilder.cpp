#include "WebApplicationBuilder.hpp"

#include "LoggingMiddleware.hpp"
#include "WebApplication.hpp"

WebApplication WebApplicationBuilder::Build() const
{
    return WebApplication(mServiceCollection).Use<LoggingMiddleware>();
}
