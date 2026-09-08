#pragma once
#include <concepts>
#include "context.hpp"

namespace asyncrt::core {

template<class T>
concept CoroutineExceptionHandler = requires(std::exception_ptr exception_ptr) {
    { T::handle(exception_ptr) };
};

template<class T, class ContextType>
concept TaskTraits = requires {
    typename std::coroutine_traits<T>;
    { T::is_cancellable() } -> std::same_as<bool>;
    { T::ResultType };
    { T::InitialSuspendAwaitableType };
    { T::FinalSuspendAwaitableType };
    { T::ExceptionHandler } -> CoroutineExceptionHandler;
} && Context<ContextType>;

}
