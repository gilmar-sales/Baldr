/**
 * @file Http/Tuple.hpp
 * @brief Compile-time reflection helpers used to introspect route handler
 *        signatures (argument types, return type) and bind dependencies
 *        resolved from the request-scoped service provider.
 */

#pragma once
#include <Baldr/Detail/Namespace.hpp>

#include <tuple>
#include <utility>

#include <Baldr/Http/Request.hpp>
#include <Baldr/Http/Response.hpp>

namespace BALDR_NAMESPACE {

/**
 * @brief Build a parallel tuple of @c T* pointers from a tuple type.
 *
 * Used to seed @ref transformTuple with the right element kinds
 * (pointer for @ref HttpRequest/@ref HttpResponse, otherwise pointer for
 * service resolution).
 */
template <typename... Ts>
auto TupleOfPtr(std::tuple<Ts...>*)
{
    return std::tuple<std::remove_const_t<std::remove_reference_t<Ts>>*...>();
}

/**
 * @brief Trait that exposes the argument and return types of a callable.
 *
 * Specialisations cover plain function types, function pointers, lambda
 * @c operator() (const and non-const, @c noexcept variants) and member
 * function pointers.
 */
template <typename T>
struct LambdaTraits;

/**
 * @brief Specialisation for plain function types.
 */
template <typename Ret, typename... Args>
struct LambdaTraits<Ret(Args...)>
{
    using ArgsTuple = std::tuple<Args...>; ///< Tuple of the callable's argument types.
    using RetType   = Ret;                 ///< The callable's return type.
};

/**
 * @brief Specialisation for function pointers.
 */
template <typename Ret, typename... Args>
struct LambdaTraits<Ret (*)(Args...)> : LambdaTraits<Ret(Args...)>
{
};

/**
 * @brief Specialisation that pulls @c operator() out of a lambda or
 *        @c std::function-like type.
 */
template <typename T>
struct LambdaTraits : LambdaTraits<decltype(&T::operator())>
{
};

/**
 * @brief Specialisation for @c const @c operator() member function pointers.
 */
template <typename T, typename Ret, typename... Args>
struct LambdaTraits<Ret (T::*)(Args...) const> : LambdaTraits<Ret(Args...)>
{
};

template <typename T, typename Ret, typename... Args>
struct LambdaTraits<Ret (T::*)(Args...) const noexcept>
    : LambdaTraits<Ret(Args...)>
{
};

template <typename T, typename Ret, typename... Args>
struct LambdaTraits<Ret (T::*)(Args...)> : LambdaTraits<Ret(Args...)>
{
};

template <typename T, typename Ret, typename... Args>
struct LambdaTraits<Ret (T::*)(Args...) noexcept> : LambdaTraits<Ret(Args...)>
{
};

/**
 * @brief Convenience alias for a callable's argument tuple type.
 */
template <typename TLambda>
using LambdaArgs =
    typename LambdaTraits<std::remove_reference_t<TLambda>>::ArgsTuple;

/**
 * @brief Convenience alias for a callable's return type.
 */
template <typename TLambda>
using LambdaResult =
    typename LambdaTraits<std::remove_reference_t<TLambda>>::RetType;

/**
 * @brief Dispatch helper for request/response arguments.
 *
 * For the request and response elements, return a reference obtained via
 * @p func. The framework binds these to the live @c HttpRequest and
 * @c HttpResponse for the in-flight call.
 */
template <typename TElement>
    requires(std::is_same_v<TElement*, HttpRequest*> ||
             std::is_same_v<TElement*, HttpResponse*>)
auto construct(auto func, auto ptrFunc, TElement* element) -> auto&
{
    return func(element);
}

/**
 * @brief Dispatch helper for service-provider-resolved arguments.
 *
 * For other arguments, return a service resolved via @p ptrFunc.
 */
template <typename TElement>
    requires(!std::is_same_v<TElement, HttpRequest*> &&
             !std::is_same_v<TElement, HttpResponse*>)
auto construct(auto func, auto ptrFunc, TElement element) -> auto
{
    return ptrFunc(element);
}

/**
 * @brief Implementation of @ref transformTuple that expands the index
 *        sequence.
 */
template <typename TupleResult, typename Func, typename PtrFunc, typename Tuple,
          std::size_t... Indices>
TupleResult transformTupleImpl(Func func, PtrFunc ptrFunc, Tuple&& tuple,
                               std::index_sequence<Indices...>)
{
    return TupleResult(construct(
        func, ptrFunc, std::get<Indices>(std::forward<Tuple>(tuple)))...);
}

/**
 * @brief Build a tuple by applying @p func or @p ptrFunc to each element
 *        of @p tuple, dispatching through @ref construct.
 *
 * @tparam TupleResult Tuple type produced by the transformation.
 * @param func    Invoked for @c HttpRequest/@c HttpResponse elements.
 * @param ptrFunc Invoked for service-provider-resolved elements.
 * @param tuple   Source tuple of pointers describing each element kind.
 */
template <typename TupleResult, typename Func, typename PtrFunc, typename Tuple>
auto transformTuple(Func func, PtrFunc ptrFunc, Tuple&& tuple)
{
    constexpr std::size_t tupleSize =
        std::tuple_size<std::remove_reference_t<Tuple>>::value;
    return transformTupleImpl<TupleResult>(
        func, ptrFunc, std::forward<Tuple>(tuple),
        std::make_index_sequence<tupleSize> {});
}

} // namespace BALDR_NAMESPACE
