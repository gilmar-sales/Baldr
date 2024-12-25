#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

template <typename T>
struct decay_param {
    using type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

template <typename T>
struct lambda_traits;

template <typename ClassType, typename ReturnType, typename... Args>
struct lambda_traits<ReturnType(ClassType::*)(Args...) const> {
    using args_tuple = std::tuple<typename decay_param<Args>::type...>;
};

template <typename Lambda>
auto get_lambda_params_as_tuple(Lambda&& lambda) {
    using lambda_type = decltype(&std::remove_reference_t<Lambda>::operator());
    return typename lambda_traits<lambda_type>::args_tuple{};
}

template <typename F, typename Tuple, std::size_t... I>
auto invoke_with_defaults(F&& func, Tuple&& tuple, std::index_sequence<I...>) {
    return func(std::get<I>(tuple)...);
}

template <typename F>
auto invoke_with_default_args(F&& func) {
    using args_tuple = decltype(get_lambda_params_as_tuple(func));
    args_tuple default_args{};
    return invoke_with_defaults(std::forward<F>(func), default_args, std::make_index_sequence<std::tuple_size<args_tuple>::value>{});
}