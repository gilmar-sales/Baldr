#pragma once

#include <coroutine>
#include <utility>

#include <Skirnir/Async/Scheduler.hpp>
#include <Skirnir/Async/Task.hpp>
#include <Skirnir/Common/Namespace.hpp>
#include <asio/any_io_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/post.hpp>

/**
 * @brief Scheduler that resumes a coroutine on a captured
 *        @c asio::any_io_executor.
 */
class AsioScheduler
{
  public:
    explicit AsioScheduler(asio::any_io_executor executor) noexcept :
        mExecutor(std::move(executor))
    {
    }

    AsioScheduler(const AsioScheduler&) noexcept = default;
    AsioScheduler(AsioScheduler&&) noexcept      = default;

    void schedule(std::coroutine_handle<> h) const
    {
        asio::post(mExecutor, [h]() mutable {
            if (h)
                h.resume();
        });
    }

    [[nodiscard]] const asio::any_io_executor& executor() const noexcept
    {
        return mExecutor;
    }

  private:
    asio::any_io_executor mExecutor;
};

/**
 * @brief Awaits an asio asynchronous operation and resumes the
 *        calling coroutine on the operation's executor.
 */
template <typename CompletionToken>
class AsioAwaitable
{
  public:
    explicit AsioAwaitable(CompletionToken token) : mToken(std::move(token)) {}

    bool await_ready() const noexcept { return false; }

    template <typename Scheduler>
    void await_suspend(std::coroutine_handle<> h, Scheduler sched)
    {
        asio::async_initiate<CompletionToken, void()>(
            [h, sched](auto&&...) { sched.schedule(h); },
            mToken);
    }

    void await_resume() const noexcept {}

  private:
    CompletionToken mToken;
};
