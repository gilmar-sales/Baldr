#pragma once

#include <atomic>
#include <memory>
#include <vector>

template <typename T, size_t BufferSize>
class MPMCLockFreePool
{
  private:
    struct Node
    {
        std::atomic<T*>     data { nullptr };
        std::atomic<size_t> sequence { 0 };
    };

    static constexpr size_t CACHE_LINE_SIZE = 64;

    alignas(CACHE_LINE_SIZE) std::atomic<size_t> enqueue_pos_ { 0 };
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> dequeue_pos_ { 0 };

    const size_t            capacity_;
    const size_t            mask_;
    std::unique_ptr<Node[]> nodes_;

  public:
    MPMCLockFreePool(size_t capacity) :
        capacity_(next_power_of_2(capacity)), mask_(capacity_ - 1),
        nodes_(std::make_unique<Node[]>(capacity_))
    {

        // Initialize sequence numbers
        for (size_t i = 0; i < capacity_; ++i)
        {
            nodes_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool try_push(T* item)
    {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

        for (;;)
        {
            Node&    node = nodes_[pos & mask_];
            size_t   seq  = node.sequence.load(std::memory_order_acquire);
            intptr_t diff =
                static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0)
            {
                // Slot disponível, tentar claim
                if (enqueue_pos_.compare_exchange_weak(
                        pos,
                        pos + 1,
                        std::memory_order_relaxed))
                {
                    node.data.store(item, std::memory_order_relaxed);
                    node.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0)
            {
                // Queue está cheia
                return false;
            }
            else
            {
                // Outro thread moveu enqueue_pos_, retry
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }

    T* try_pop()
    {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

        for (;;)
        {
            Node&    node = nodes_[pos & mask_];
            size_t   seq  = node.sequence.load(std::memory_order_acquire);
            intptr_t diff =
                static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0)
            {
                // Item disponível, tentar claim
                if (dequeue_pos_.compare_exchange_weak(
                        pos,
                        pos + 1,
                        std::memory_order_relaxed))
                {

                    T* data = node.data.load(std::memory_order_relaxed);
                    node.sequence.store(pos + mask_ + 1,
                                        std::memory_order_release);
                    return data;
                }
            }
            else if (diff < 0)
            {
                auto buffer = new T();
                buffer->reserve(BufferSize);
                // Queue está vazia
                return buffer;
            }
            else
            {
                // Outro thread moveu dequeue_pos_, retry
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
    }

  private:
    static size_t next_power_of_2(size_t n)
    {
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }
};

using MpMcBufferPool = MPMCLockFreePool<std::vector<char>, 16 * 1024>;
