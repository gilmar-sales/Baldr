/**
 * @file Hosting/SecureRandom.hpp
 * @brief Thread-safe cryptographic random number generator helpers.
 *
 * Backed by @c std::random_device (which is non-deterministic on the
 * platforms Baldr supports: @c /dev/urandom on POSIX, @c BCryptGenRandom
 * on Windows) plus a per-thread @c std::mt19937_64 seeded with strong
 * entropy. Use this for any value that must be unpredictable to a
 * same-process or network-adjacent attacker: CSRF tokens, request IDs,
 * trace IDs, etc.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

namespace BALDR_NAMESPACE
{

    namespace Detail
    {
        /**
         * @brief Returns a reference to a thread-local @c mt19937_64 seeded
         *        from @c std::random_device and the steady clock.
         *
         * The first call on each thread performs seeding; subsequent calls
         * are cheap. Mixing @c random_device output with the per-thread
         * clock prevents the trivial predictability of a counter-only RNG.
         */
        inline std::mt19937_64& ThreadRng()
        {
            thread_local std::mt19937_64 rng { []() {
                std::random_device rd;
                std::seed_seq      seq {
                    static_cast<std::uint32_t>(rd()),
                    static_cast<std::uint32_t>(rd()),
                    static_cast<std::uint32_t>(rd()),
                    static_cast<std::uint32_t>(rd()),
                    static_cast<std::uint32_t>(std::chrono::steady_clock::now()
                                                   .time_since_epoch()
                                                   .count()),
                    static_cast<std::uint32_t>(std::chrono::steady_clock::now()
                                                   .time_since_epoch()
                                                   .count() >>
                                               32),
                };
                std::mt19937_64 r;
                r.seed(seq);
                return r;
            }() };
            return rng;
        }
    } // namespace Detail

    /**
     * @brief Fill @p out with @c n bytes from a cryptographically strong RNG.
     *
     * Mixes two 64-bit draws from the thread-local PRNG; the underlying
     * entropy comes from @c std::random_device at thread construction.
     */
    inline void FillRandom(unsigned char* out, std::size_t n)
    {
        auto& rng = Detail::ThreadRng();
        while (n >= sizeof(std::uint64_t))
        {
            const auto v = static_cast<std::uint64_t>(rng());
            std::memcpy(out, &v, sizeof(v));
            out += sizeof(v);
            n -= sizeof(v);
        }
        if (n > 0)
        {
            const auto v = static_cast<std::uint64_t>(rng());
            std::memcpy(out, &v, n);
        }
    }

    /**
     * @brief Generate a lowercase hexadecimal token of @p hexChars characters
     *        (must be even) from the secure RNG.
     *
     * Used by CSRF, request-id, and trace-id generators.
     */
    inline std::string RandomHex(std::size_t hexChars)
    {
        if (hexChars == 0 || (hexChars % 2) != 0)
            return {};
        const std::size_t bytes = hexChars / 2;
        std::string       s;
        s.resize(bytes);
        FillRandom(reinterpret_cast<unsigned char*>(s.data()), bytes);
        static constexpr char kHex[] = "0123456789abcdef";
        std::string           out;
        out.resize(hexChars);
        for (std::size_t i = 0; i < bytes; ++i)
        {
            const auto b   = static_cast<unsigned char>(s[i]);
            out[2 * i]     = kHex[(b >> 4) & 0x0F];
            out[2 * i + 1] = kHex[b & 0x0F];
        }
        return out;
    }

} // namespace BALDR_NAMESPACE