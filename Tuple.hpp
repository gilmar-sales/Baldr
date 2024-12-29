#pragma once

#include <tuple>
#include <utility>
#include <iostream>

template<typename... Ts>
auto TupleOfPtr(std::tuple<Ts...>* )
{
    return std::tuple<std::remove_const_t<std::remove_reference_t<Ts>>*...>();
}

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

template <typename TElement>
requires (std::is_same_v<TElement*, HttpRequest*> || std::is_same_v<TElement*, HttpResponse*>)
auto construct(auto func, auto ptrFunc, TElement* element) -> auto&
{
        return func(element);
}

template <typename TElement>
requires (!std::is_same_v<TElement, HttpRequest*> && !std::is_same_v<TElement, HttpResponse*>)
auto construct(auto func, auto ptrFunc,  TElement element) -> auto
{
    return ptrFunc(element);
}

// Helper function to apply a transformation to a tuple
template <typename TupleResult,typename Func, typename PtrFunc, typename Tuple, std::size_t... Indices>
TupleResult transformTupleImpl(Func func, PtrFunc ptrFunc, Tuple&& tuple, std::index_sequence<Indices...>) {
    return TupleResult(construct(func, ptrFunc, std::get<Indices>(std::forward<Tuple>(tuple)))...);
}

// Main function to transform a tuple
template <typename TupleResult, typename Func, typename PtrFunc, typename Tuple>
auto transformTuple(Func func, PtrFunc ptrFunc, Tuple&& tuple) {
    constexpr std::size_t tupleSize = std::tuple_size<std::remove_reference_t<Tuple>>::value;
    return transformTupleImpl<TupleResult>(func, ptrFunc, std::forward<Tuple>(tuple), std::make_index_sequence<tupleSize>{});
}
