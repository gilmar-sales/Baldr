#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Todo.hpp"

class ITodoRepository
{
  public:
    virtual ~ITodoRepository() = default;

    virtual std::vector<Todo>   List()                               = 0;
    virtual std::optional<Todo> GetById(int64_t id)                  = 0;
    virtual Todo                Create(std::string title, bool done) = 0;
    virtual std::optional<Todo> Update(int64_t id,
                                       std::optional<std::string>
                                           title,
                                       std::optional<bool>
                                           done)                     = 0;
    virtual bool                Delete(int64_t id)                   = 0;
};