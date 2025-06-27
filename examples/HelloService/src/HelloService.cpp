#include "HelloService.hpp"

Payload HelloService::Hello(std::string name)
{
    mLogger->LogDebug("Hello");
    return Payload { .message = std::format("Hello, {}!", name) };
}
