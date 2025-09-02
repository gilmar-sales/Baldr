#pragma once

#include "Net.hpp"

#include <shared_mutex>
#include <vector>

template <size_t BufferSize>
class BufferPool
{
  public:
    using Buffer        = std::vector<char>;
    using BufferHandler = std::unique_ptr<std::vector<char>>;

    BufferHandler acquire()
    {
        {
            auto write = std::unique_lock(mMutex);

            if (!mPool.empty())
            {
                auto buffer = std::move(mPool.back());

                mPool.pop_back();
                return buffer;
            }
        }

        auto buffer = std::make_unique<Buffer>();
        buffer->reserve(BufferSize);
        return std::move(buffer);
    }

    void release(BufferHandler buffer)
    {
        auto write = std::unique_lock(mMutex);
        mPool.push_back(std::move(buffer));
    }

  private:
    mutable std::shared_mutex  mMutex;
    std::vector<BufferHandler> mPool;
};

using ReadBufferPool = BufferPool<16 * 1024>;
