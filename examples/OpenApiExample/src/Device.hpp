#pragma once

#include <string>

struct Device
{
    int         id;
    std::string uuid;
    std::string mac;
    std::string firmware;
    std::string created_at;
    std::string updated_at;
};