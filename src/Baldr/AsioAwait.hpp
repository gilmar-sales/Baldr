#pragma once

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#include <Skirnir/Async/Task.hpp>

#include "Net.hpp"

namespace baldr
{
    namespace detail
    {
        template <typename T>
        struct FutureState
        {
            std::promise<T>       promise;
            std::shared_future<T> future = promise.get_future().share();
            std::mutex            mutex;
            std::coroutine_handle<> resumer {};
            bool                  consumed = false;
        };

        template <typename T>
        class FutureAwaiter
        {
          public:
            explicit FutureAwaiter(std::shared_ptr<FutureState<T>> state) :
                mState(std::move(state))
            {
            }

            bool await_ready() const noexcept { return false; }

            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<> awaiter) noexcept
            {
                {
                    std::lock_guard lock(mState->mutex);
                    mState->resumer = awaiter;
                }

                mState->future.wait();

                {
                    std::lock_guard lock(mState->mutex);
                    mState->consumed = true;
                    return mState->resumer;
                }
            }

            T await_resume()
            {
                return mState->future.get();
            }

          private:
            std::shared_ptr<FutureState<T>> mState;
        };

        template <>
        class FutureAwaiter<void>
        {
          public:
            explicit FutureAwaiter(std::shared_ptr<FutureState<void>> state) :
                mState(std::move(state))
            {
            }

            bool await_ready() const noexcept { return false; }

            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<> awaiter) noexcept
            {
                {
                    std::lock_guard lock(mState->mutex);
                    mState->resumer = awaiter;
                }

                mState->future.wait();

                {
                    std::lock_guard lock(mState->mutex);
                    mState->consumed = true;
                    return mState->resumer;
                }
            }

            void await_resume() { mState->future.get(); }

          private:
            std::shared_ptr<FutureState<void>> mState;
        };
    } // namespace detail

    template <typename T>
    skr::Task<T> AsioAwait(net::any_io_executor executor,
                           net::awaitable<T>   awaitable)
    {
        auto state = std::make_shared<detail::FutureState<T>>();

        net::co_spawn(
            executor,
            [state, awaitable = std::move(awaitable)]() mutable
                -> net::awaitable<void>
            {
                try
                {
                    if constexpr (std::is_void_v<T>)
                    {
                        co_await std::move(awaitable);
                        state->promise.set_value();
                    }
                    else
                    {
                        state->promise.set_value(
                            co_await std::move(awaitable));
                    }
                }
                catch (...)
                {
                    state->promise.set_exception(std::current_exception());
                }
            },
            net::detached);

        co_await detail::FutureAwaiter<T>(state);

        if constexpr (!std::is_void_v<T>)
        {
            co_return state->future.get();
        }
        else
        {
            co_return;
        }
    }
}
