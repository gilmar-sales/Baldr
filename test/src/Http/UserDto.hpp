#pragma once

#include <string>

struct UserDto
{
    std::string name;
    int         age = 0;
};

struct IdArg
{
    std::string id;
};