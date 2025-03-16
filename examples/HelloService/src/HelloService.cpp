#include "HelloService.hpp"

Payload HelloService::Hello()
{
    mLogger->LogDebug("Hello");
    return Payload { .message = "Hello, World!" };
}
