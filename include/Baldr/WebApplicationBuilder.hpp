#pragma once

#include <Skirnir/Skirnir.hpp>

class WebApplication;

class WebApplicationBuilder
{
  public:
    [[nodiscard]] skr::ServiceCollection& GetServiceCollection() const
    {
        return *mServiceCollection;
    }

    [[nodiscard]] WebApplication Build() const;

    template <typename Callable>
    WebApplicationBuilder& operator|(Callable callable)
    {
        return callable(*this);
    }

  protected:
    WebApplicationBuilder() :
        mServiceCollection(std::make_shared<skr::ServiceCollection>())
    {
    }

    friend class WebApplication;

  private:
    Ref<skr::ServiceCollection> mServiceCollection;
};
