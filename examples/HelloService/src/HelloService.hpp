#pragma once

#include <Skirnir/Skirnir.hpp>

struct Payload
{
    std::string message;
};

class HelloService
{
  public:
    HelloService(const skr::Arc<skr::Logger<HelloService>> logger) :
        mLogger(logger)
    {
    }

    Payload Hello(std::string name);

  private:
    skr::Arc<skr::Logger<HelloService>> mLogger;
};