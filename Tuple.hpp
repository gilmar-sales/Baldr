#pragma once

#include <tuple>
#include <utility>
#include <iostream>

// Helper to extract lambda's argument types
template <typename T>
struct LambdaTraits;

// Specialization for callable objects (lambdas, std::function, etc.)
template <typename Ret, typename... Args>
struct LambdaTraits<Ret(Args...)> {
    using ArgsTuple = std::tuple<std::remove_const_t<std::remove_reference_t<Args>>*...>;
};

// Specialization for function pointers
template <typename Ret, typename... Args>
struct LambdaTraits<Ret(*)(Args...)> : LambdaTraits<Ret(Args...)> {};

// Deduce lambda types
template <typename T>
struct LambdaTraits : LambdaTraits<decltype(&T::operator())> {};

// Specialization for member function pointers
template <typename T, typename Ret, typename... Args>
struct LambdaTraits<Ret(T::*)(Args...) const> : LambdaTraits<Ret(Args...)> {};


// Helper function to apply a transformation to a tuple
template <typename Func, typename Tuple, std::size_t... Indices>
auto transformTupleImpl(Func func, Tuple&& tuple, std::index_sequence<Indices...>) {
    return std::tie(func(std::get<Indices>(std::forward<Tuple>(tuple)))...);
}

// Main function to transform a tuple
template <typename Func, typename Tuple>
auto transformTuple(Func func, Tuple&& tuple) {
    constexpr std::size_t tupleSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
    return transformTupleImpl(func, std::forward<Tuple>(tuple), std::make_index_sequence<tupleSize>{});
}
