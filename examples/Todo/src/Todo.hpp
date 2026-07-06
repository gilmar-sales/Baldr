#pragma once

#include <cstdint>
#include <string>

struct Todo
{
    int64_t     id;
    std::string title;
    bool        done;
    std::string created_at;
};

struct CreateTodoDto
{
    std::string title;
    bool        done = false;
};

struct UpdateTodoDto
{
    std::string title;
    bool        done;
};

struct IdParam
{
    int64_t id;
};