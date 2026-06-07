#include <Baldr/AsioAdapter.hpp>
#include <Skirnir/Async.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>

#include "gtest/gtest.h"

TEST(AsioAdapterSpec, AsioSchedulerResumesOnExecutor)
{
    asio::io_context  ctx;
    AsioScheduler     sched(ctx.get_executor());
    std::atomic<bool> ran { false };

    auto task = [&]() -> skr::Task<> {
        ran.store(true);
        co_return;
    }();
    sched.schedule(task.handle());
    EXPECT_FALSE(ran.load());
    ctx.run();
    EXPECT_TRUE(ran.load());
}

TEST(AsioAdapterSpec, AsioAwaitableRoundtripsErrorCode)
{
    asio::io_context ctx;
    AsioScheduler    sched(ctx.get_executor());

    std::promise<std::error_code> done;
    auto                          fut = done.get_future();

    asio::steady_timer timer(ctx, std::chrono::milliseconds(5));
    timer.async_wait([&done](std::error_code ec) { done.set_value(ec); });

    std::thread t([&]() { ctx.run(); });
    auto        ec = fut.get();
    t.join();
    EXPECT_FALSE(ec);
}
