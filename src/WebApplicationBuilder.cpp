#include "Baldr/WebApplicationBuilder.hpp"

#include "Baldr/LoggingMiddleware.hpp"
#include "Baldr/WebApplication.hpp"

WebApplication WebApplicationBuilder::Build() const
{
    return WebApplication(mServiceCollection).Use<LoggingMiddleware>();
}
