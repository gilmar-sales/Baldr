#pragma once

#include "Baldr/StatusCode.hpp"

template <typename TValue>
struct HttpResult
{
    bool        success = false;
    std::string error;
    StatusCode  statusCode = StatusCode::InternalServerError;
    TValue      value;
};
