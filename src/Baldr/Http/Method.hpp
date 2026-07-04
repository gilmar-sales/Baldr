#pragma once
#include <Baldr/Detail/Namespace.hpp>

namespace BALDR_NAMESPACE {

enum class HttpMethod : int
{
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Options,
    Head,
    Trace,
    Connect
};

} // namespace BALDR_NAMESPACE
