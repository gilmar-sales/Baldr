#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Application/WebApplication.hpp>

namespace BALDR_NAMESPACE {

class BaldrExtension : public skr::IExtension
{
  public:
    virtual ~BaldrExtension() = default;

    virtual void ConfigureServices(skr::ServiceCollection& services) override;
    virtual void UseServices(skr::ServiceProvider& serviceProvider) override;
};

} // namespace BALDR_NAMESPACE
