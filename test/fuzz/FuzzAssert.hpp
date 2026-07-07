#ifndef BALDR_TEST_FUZZ_FUZZASSERT_HPP
#define BALDR_TEST_FUZZ_FUZZASSERT_HPP

#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(BALDR_FUZZ_VERBOSE) && BALDR_FUZZ_VERBOSE
    #define BALDR_FUZZ_VERBOSE_ENABLED 1
#else
    #define BALDR_FUZZ_VERBOSE_ENABLED 0
#endif

namespace baldr::fuzz
{
    struct FuzzSlot
    {
        std::string_view          name;
        std::vector<std::uint8_t> bytes;
        std::string_view          as_string;
    };

    class FuzzRecorder
    {
      public:
        explicit FuzzRecorder(std::span<const std::uint8_t> raw) : raw_(raw) {}

        void record(std::string_view name, std::span<const std::uint8_t> bytes)
        {
            FuzzSlot s;
            s.name = name;
            s.bytes.assign(bytes.begin(), bytes.end());
            s.as_string = std::string_view(
                reinterpret_cast<const char*>(s.bytes.data()), s.bytes.size());
            slots_.push_back(std::move(s));
        }

        [[noreturn]] void report(std::string_view what_failed) const
        {
            std::fprintf(stderr, "==== BALDR FUZZ PROPERTY FAILURE ====\n");
            std::fprintf(stderr, "Property: %s\n",
                         std::string(what_failed).c_str());
            std::fprintf(stderr, "Raw input (%zu bytes):\n  ", raw_.size());
            for (std::uint8_t b : raw_)
                std::fprintf(stderr, "%02x ", b);
            std::fprintf(stderr, "\nRecorded slots (%zu):\n", slots_.size());
            for (const auto& s : slots_)
            {
                std::fprintf(stderr, "  - %.*s (%zu bytes): ",
                             static_cast<int>(s.name.size()), s.name.data(),
                             s.bytes.size());
                for (std::uint8_t b : s.bytes)
                    std::fprintf(stderr, "%02x ", b);
                std::fprintf(stderr, "\n");
            }
            std::fprintf(stderr,
                         "==== TRAP (afl++ will record unique_crash) ====\n");
            std::fflush(stderr);
            __builtin_trap();
        }

      private:
        std::span<const std::uint8_t> raw_;
        std::vector<FuzzSlot>         slots_;
    };

    inline FuzzRecorder* g_active_recorder = nullptr;
} // namespace baldr::fuzz

#if BALDR_FUZZ_VERBOSE_ENABLED
    #define BDR_RECORD(fdp, value, name_literal)                               \
        do                                                                     \
        {                                                                      \
            if (::baldr::fuzz::g_active_recorder)                              \
            {                                                                  \
                auto _bdr_v = (value);                                         \
                ::baldr::fuzz::g_active_recorder->record(                      \
                    (name_literal),                                            \
                    std::span<const std::uint8_t>(                             \
                        reinterpret_cast<const std::uint8_t*>(_bdr_v.data()),  \
                        _bdr_v.size()));                                       \
            }                                                                  \
        } while (0)

    #define BDR_ASSERT(rec, cond, msg)                                         \
        do                                                                     \
        {                                                                      \
            if (!(cond))                                                       \
            {                                                                  \
                (rec).report(msg);                                             \
            }                                                                  \
        } while (0)
#else
    #define BDR_RECORD(fdp, value, name_literal)                               \
        do                                                                     \
        {                                                                      \
        } while (0)
    #define BDR_ASSERT(rec, cond, msg)                                         \
        do                                                                     \
        {                                                                      \
            if (!(cond))                                                       \
            {                                                                  \
                std::fprintf(stderr, "BDR_ASSERT failed at %s:%d: %s\n",       \
                             __FILE__, __LINE__, msg);                         \
                __builtin_trap();                                              \
            }                                                                  \
        } while (0)
#endif

#endif