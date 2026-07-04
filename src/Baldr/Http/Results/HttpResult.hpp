#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <Baldr/Http/StatusCode.hpp>

namespace BALDR_NAMESPACE {

template <typename TValue>
struct HttpResult
{
    bool        success = false;
    std::string error;
    StatusCode  statusCode = StatusCode::InternalServerError;
    TValue      value;
};

} // namespace BALDR_NAMESPACE
