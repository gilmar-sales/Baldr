#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ITodoRepository.hpp"

class InMemoryTodoRepository final : public ITodoRepository
{
  public:
    InMemoryTodoRepository()           = default;
    ~InMemoryTodoRepository() override = default;

    InMemoryTodoRepository(const InMemoryTodoRepository&)            = delete;
    InMemoryTodoRepository& operator=(const InMemoryTodoRepository&) = delete;

    std::vector<Todo> List(int limit, int offset) override;

    long long Count() override;

    std::optional<Todo> GetById(int64_t id) override;

    Todo Create(std::string title, bool done) override;

    std::optional<Todo> Update(int64_t id, std::optional<std::string> title,
                               std::optional<bool> done) override;

    bool Delete(int64_t id) override;

  private:
    std::mutex                        mMutex;
    std::unordered_map<int64_t, Todo> mTodos;
    std::atomic<int64_t>              mNextId { 1 };
};