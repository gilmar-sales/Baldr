#pragma once

#include <Skirnir/Skirnir.hpp>

struct Payload
{
    std::string message;
};

class HelloService
{
  public:
    HelloService(const Ref<skr::Logger<HelloService>> logger) : mLogger(logger)
    {
    }

    Payload Hello();

  private:
    Ref<skr::Logger<HelloService>> mLogger;
};