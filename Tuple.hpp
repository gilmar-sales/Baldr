#include <tuple>
#include <utility>
#include <iostream>

// Helper to extract lambda's argument types
template <typename T>
struct LambdaTraits;

// Specialization for callable objects (lambdas, std::function, etc.)
template <typename Ret, typename... Args>
struct LambdaTraits<Ret(Args...)> {
    using ArgsTuple = std::tuple<Args...>;
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