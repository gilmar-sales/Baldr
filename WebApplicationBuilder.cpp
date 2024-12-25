#include "WebApplicationBuilder.hpp"
#include "WebApplication.hpp"

WebApplication WebApplicationBuilder::Build() const {
    return WebApplication(mServiceCollection);
}
