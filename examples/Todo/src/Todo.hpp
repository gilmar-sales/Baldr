#pragma once

#include <cstdint>
#include <optional>
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
    std::optional<std::string> title;
    std::optional<bool>        done;
};

struct IdParam
{
    int64_t id;
};