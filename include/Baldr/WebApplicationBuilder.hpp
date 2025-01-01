#pragma once

#include <Skirnir.hpp>

class WebApplication;

class WebApplicationBuilder
{
  public:
    [[nodiscard]] ServiceCollection& GetServiceCollection() const
    {
        return *mServiceCollection;
    }

  [[nodiscard]] WebApplication Build() const;

  template <typename Callable>
  WebApplicationBuilder& operator|(Callable callable) {
    return callable(*this);
  }

  protected:
    WebApplicationBuilder() :
        mServiceCollection(std::make_shared<ServiceCollection>())
    {
    }

    friend class WebApplication;

  private:
    std::shared_ptr<ServiceCollection> mServiceCollection;
};
