#pragma once
#include <concepts>
#include <coroutine>
#include <functional>

namespace asyncrt::core {

struct PostCoroutineTag {};
inline constexpr PostCoroutineTag post_coroutine;

template<class T>
concept Validatable = std::convertible_to<T, bool> || std::predicate<T>;

template<class T>
concept Context = requires(T t, std::coroutine_handle<> handle, std::move_only_function<void()> callable, PostCoroutineTag post_coroutine_tag) {
    // send - sends a handle or an invokable to the context and waits for it to finish
    // static variants shortcut to the current context (convenience)
    //{ t.send(post_coroutine_tag, handle) } -> Validatable;
    //{ t.send(std::move(callable)) } -> Validatable;

    // post - posts a handle or an invokable to the context without waiting
    // static variants shortcut to the current context (convenience)
    { t.post(post_coroutine_tag, handle) } -> Validatable;
    { t.post(std::move(callable)) } -> Validatable;

    // stop the context, meaning no coroutines can be scheduled anymore
    { t.stop() };
} && std::default_initializable<T>;

}

