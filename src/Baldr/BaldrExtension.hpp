#pragma once

#include <Skirnir/Skirnir.hpp>

#include "WebApplication.hpp"

class BaldrExtension : public skr::IExtension
{
  public:
    virtual ~BaldrExtension() = default;

    virtual void ConfigureServices(skr::ServiceCollection& services) override;
    virtual void UseServices(skr::ServiceProvider& serviceProvider) override;
};
