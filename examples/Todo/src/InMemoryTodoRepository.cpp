#include "InMemoryTodoRepository.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>

namespace
{
    std::string nowIso8601()
    {
        return std::format("{:%Y-%m-%dT%H:%M:%SZ}",
                           std::chrono::system_clock::now());
    }
} // namespace

std::vector<Todo> InMemoryTodoRepository::List(int limit, int offset)
{
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<Todo>           sorted;
    sorted.reserve(mTodos.size());
    for (const auto& [id, todo] : mTodos)
        sorted.push_back(todo);
    std::sort(sorted.begin(), sorted.end(),
              [](const Todo& a, const Todo& b) { return a.id < b.id; });

    if (offset < 0)
        offset = 0;
    if (limit < 0)
        limit = 0;
    if (offset >= static_cast<int>(sorted.size()))
        return {};

    int end = offset + limit;
    if (end > static_cast<int>(sorted.size()))
        end = static_cast<int>(sorted.size());

    return std::vector<Todo>(sorted.begin() + offset, sorted.begin() + end);
}

long long InMemoryTodoRepository::Count()
{
    std::lock_guard<std::mutex> lock(mMutex);
    return static_cast<long long>(mTodos.size());
}

std::optional<Todo> InMemoryTodoRepository::GetById(int64_t id)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto                        it = mTodos.find(id);
    if (it == mTodos.end())
        return std::nullopt;
    return it->second;
}

Todo InMemoryTodoRepository::Create(std::string title, bool done)
{
    Todo todo;
    todo.id         = mNextId.fetch_add(1);
    todo.title      = std::move(title);
    todo.done       = done;
    todo.created_at = nowIso8601();
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mTodos.emplace(todo.id, todo);
    }
    return todo;
}

std::optional<Todo> InMemoryTodoRepository::Update(
    int64_t id, std::optional<std::string> title, std::optional<bool> done)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto                        it = mTodos.find(id);
    if (it == mTodos.end())
        return std::nullopt;
    if (title)
        it->second.title = std::move(*title);
    if (done)
        it->second.done = *done;
    return it->second;
}

bool InMemoryTodoRepository::Delete(int64_t id)
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mTodos.erase(id) > 0;
}