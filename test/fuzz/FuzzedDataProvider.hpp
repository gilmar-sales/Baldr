//===- FuzzedDataProvider.hpp - Header-only FDP ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A header-only port of llvm::FuzzedDataProvider trimmed to the methods used
// by Baldr's afl++ harnesses. The original lives at
// compiler-rt/include/fuzzer/FuzzedDataProvider.h; the upstream license header
// is preserved above.
//
// This shim exists because afl++ has no built-in FDP equivalent, and using a
// shared provider across all harnesses keeps mutation semantics consistent
// between the in-tree gtest fuzz-specs and the afl++ lane.
//
//===----------------------------------------------------------------------===//
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace baldr::fuzz
{

    class FuzzedDataProvider
    {
      public:
        FuzzedDataProvider(const uint8_t* data, std::size_t size)
            : data_ptr_(data), remaining_bytes_(size)
        {
        }

        explicit FuzzedDataProvider(std::span<const uint8_t> span)
            : data_ptr_(span.data()), remaining_bytes_(span.size())
        {
        }

        std::span<const uint8_t> ConsumeBytes(std::size_t num_bytes)
        {
            num_bytes = std::min(num_bytes, remaining_bytes_);
            auto result =
                std::span<const uint8_t>(data_ptr_, num_bytes);
            Advance(num_bytes);
            return result;
        }

        std::span<const uint8_t> ConsumeRemainingBytes()
        {
            return ConsumeBytes(remaining_bytes_);
        }

        std::vector<uint8_t> ConsumeBytesAsVector(std::size_t num_bytes)
        {
            auto bytes = ConsumeBytes(num_bytes);
            return std::vector<uint8_t>(bytes.begin(), bytes.end());
        }

        std::string ConsumeBytesAsString(std::size_t num_bytes)
        {
            auto bytes = ConsumeBytes(num_bytes);
            return std::string(reinterpret_cast<const char*>(bytes.data()),
                               bytes.size());
        }

        std::string ConsumeRandomLengthString(std::size_t max_length)
        {
            // Choose a length in [0, max_length], biased toward shorter values.
            auto length =
                ConsumeIntegralInRange<std::size_t>(0, max_length);
            return ConsumeBytesAsString(length);
        }

        std::string ConsumeRemainingBytesAsString()
        {
            return ConsumeBytesAsString(remaining_bytes_);
        }

        bool ConsumeBool()
        {
            return 1 & ConsumeIntegral<uint8_t>();
        }

        template <typename T>
            requires std::is_integral_v<T> && (sizeof(T) <= 8)
        T ConsumeIntegral()
        {
            return ConsumeIntegralInRange<T>(std::numeric_limits<T>::min(),
                                             std::numeric_limits<T>::max());
        }

        template <typename T>
            requires std::is_integral_v<T> && (sizeof(T) <= 8)
        T ConsumeIntegralInRange(T min, T max)
        {
            if (min > max)
                min = max;

            // Avoid division-by-zero.
            if (min == max)
                return min;

            // Use the biggest type possible to hold the range and the result.
            using UIntType =
                std::conditional_t<std::is_signed_v<T>,
                                   std::make_unsigned_t<T>,
                                   T>;

            // Compute the inclusive range [range_begin, range_end].
            UIntType range_begin  = static_cast<UIntType>(min);
            UIntType range_end    = static_cast<UIntType>(max);
            UIntType range_size   = static_cast<UIntType>(range_end - range_begin);
            UIntType result       = 0;

            // Consume bytes until we have enough bits to cover range_size.
            std::size_t offset = 0;
            while (offset < sizeof(T) * 8 &&
                   (range_size >> offset) > 0)
            {
                ++offset;
            }
            auto needed_bytes = (offset + 7) / 8;
            if (needed_bytes == 0)
                needed_bytes = 1;

            auto consumed = ConsumeBytes(needed_bytes);
            for (std::size_t i = 0; i < consumed.size(); ++i)
            {
                result = (result << 8) |
                         static_cast<UIntType>(consumed[i]);
            }
            // Avoid bias by folding the random space.
            if (range_size != std::numeric_limits<UIntType>::max())
                result = result % (range_size + 1);

            return static_cast<T>(range_begin + result);
        }

        template <typename T>
            requires std::is_floating_point_v<T>
        T ConsumeFloatingPoint()
        {
            return ConsumeFloatingPointInRange<T>(
                std::numeric_limits<T>::lowest(),
                std::numeric_limits<T>::max());
        }

        template <typename T>
            requires std::is_floating_point_v<T>
        T ConsumeFloatingPointInRange(T min, T max)
        {
            if (std::isnan(min) || std::isnan(max))
                return std::numeric_limits<T>::quiet_NaN();

            T range = max - min;
            T result = static_cast<T>(ConsumeProbability<T>()) * range + min;
            if (result > max)
                result = max;
            return result;
        }

        template <typename T>
            requires std::is_floating_point_v<T>
        T ConsumeProbability()
        {
            using UIntType =
                std::conditional_t<sizeof(T) == sizeof(double),
                                   std::uint64_t,
                                   std::uint32_t>;
            UIntType raw = ConsumeIntegral<UIntType>();
            if (raw == std::numeric_limits<UIntType>::max())
                return static_cast<T>(1);
            return static_cast<T>(raw) /
                   static_cast<T>(std::numeric_limits<UIntType>::max());
        }

        template <typename T>
        T ConsumeEnum()
        {
            return static_cast<T>(
                ConsumeIntegralInRange<std::size_t>(0,
                                                    NumEnumValues<T>() - 1));
        }

        template <typename T, std::size_t N>
        T PickValueInArray(const std::array<T, N>& array)
        {
            if (array.empty())
                __builtin_trap();
            auto idx = ConsumeIntegralInRange<std::size_t>(0,
                                                           array.size() - 1);
            return array[idx];
        }

        template <typename T>
        T PickValueInArray(std::span<const T> array)
        {
            if (array.empty())
                __builtin_trap();
            auto idx = ConsumeIntegralInRange<std::size_t>(0,
                                                           array.size() - 1);
            return array[idx];
        }

        template <typename T>
            requires std::is_enum_v<T>
        std::optional<T> ConsumeEnumInArray(
            std::initializer_list<T> values)
        {
            if (values.size() == 0)
                return std::nullopt;
            auto index = ConsumeIntegralInRange<std::size_t>(0,
                                                              values.size() - 1);
            return *(values.begin() + index);
        }

      private:
        static constexpr std::size_t NumEnumValues() { return 1; }

        template <typename T>
        static constexpr std::size_t NumEnumValues()
        {
            return static_cast<std::size_t>(
                       std::numeric_limits<
                           std::underlying_type_t<T>>::max()) -
                   static_cast<std::size_t>(
                       std::numeric_limits<
                           std::underlying_type_t<T>>::min()) +
                   1;
        }

        void Advance(std::size_t num_bytes)
        {
            if (num_bytes > remaining_bytes_)
                num_bytes = remaining_bytes_;
            data_ptr_ += num_bytes;
            remaining_bytes_ -= num_bytes;
        }

        const uint8_t* data_ptr_;
        std::size_t remaining_bytes_;
    };

} // namespace baldr::fuzz