/**
 * @file Hosting/MpMcPool.hpp
 * @brief Bounded multi-producer / multi-consumer lock-free ring buffer
 *        used to recycle per-connection I/O buffers without contention.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <atomic>
#include <memory>
#include <vector>

namespace BALDR_NAMESPACE
{

    /**
     * @brief Fixed-capacity, power-of-two, single-slot-per-cell MPMC queue.
     *
     * Implemented as a Vyukov-style bounded MPMC ring with per-slot
     * sequence numbers to coordinate producers and consumers without a
     * global lock. Capacity is rounded up to the next power of two.
     *
     * @tparam T          Element type held in the pool. Must be
     *                    default-constructible.
     * @tparam BufferSize Hint passed to newly created elements when the
     *                    pool is empty (e.g. reserve size of a vector).
     */
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
        /**
         * @brief Construct a pool with the given (rounded-up) capacity.
         *
         * @param capacity Minimum number of slots; rounded up to the
         *                 next power of two.
         */
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

        /**
         * @brief Attempt to enqueue @p item without blocking.
         *
         * @param item Pointer to the element to enqueue. Ownership is not
         *             transferred; the caller must keep @p item alive
         *             until it is consumed.
         * @return @c true if the item was queued, @c false if the pool is
         *         full.
         */
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

        /**
         * @brief Attempt to dequeue an element without blocking.
         *
         * @return A pointer previously pushed via @ref try_push, or a
         *         freshly default-constructed element when the pool is
         *         empty (callers should be prepared to handle either).
         */
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

    /**
     * @brief Specialised pool of @c std::vector@<char@> buffers pre-sized to
     *        16 KiB. Used by the HTTP connection layer to recycle read
     *        buffers across requests without locking.
     */
    using MpMcBufferPool = MPMCLockFreePool<std::vector<char>, 16 * 1024>;

} // namespace BALDR_NAMESPACE
