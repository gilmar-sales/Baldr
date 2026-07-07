#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

struct PageQuery
{
    std::optional<int> pageSize;
    std::optional<int> page;

    struct Normalized
    {
        int pageSize;
        int page;
        int offset;
    };

    Normalized normalized() const noexcept
    {
        int ps = pageSize.value_or(500);
        if (ps < 1)
            ps = 1;
        if (ps > 500)
            ps = 500;
        int p = page.value_or(1);
        if (p < 1)
            p = 1;
        return Normalized { .pageSize = ps, .page = p, .offset = (p - 1) * ps };
    }
};

template <typename T>
struct Page
{
    std::vector<T> items;
    int            page;
    int            pageSize;
    int64_t        total;
};

struct TodoPage
{
    std::vector<Todo> items;
    int               page;
    int               pageSize;
    int64_t           total;
};