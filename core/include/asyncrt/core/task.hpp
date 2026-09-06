#pragma once
#include <exception>
#include <future>
#include <concepts>
#include <coroutine>

#include "context.hpp"
#include "detail/coro_utils.hpp"

namespace asyncrt::core {

template<class T, Context ContextType>
class Task;

namespace detail {

enum TaskFlags : std::uint8_t { // increase size whenever necessary
    TASK_DETACHED = 1 << 0,
    TASK_RESUME_ON_CAPTURED_CONTEXT = 1 << 1,
    TASK_CANCELLED = 1 << 2,
    // more options
};

// default flags; will resume on captured context by default
inline constexpr TaskFlags DEFAULT_TASK_FLAGS {
    TASK_RESUME_ON_CAPTURED_CONTEXT
};

template<class T, Context ContextType>
struct TaskPromiseBase {
    [[nodiscard]] auto initial_suspend() const noexcept -> std::suspend_always;
    auto final_suspend() const noexcept;
    auto get_return_object(this auto &self) noexcept -> Task<T, ContextType>;
    void unhandled_exception();

    [[nodiscard]]
    bool is_detached() const noexcept;
    void detach() noexcept;

    std::coroutine_handle<> _continuation{};
    std::unique_ptr<ContextType> _captured_ctx;
    std::promise<T> _state;
    std::uint8_t _flags{DEFAULT_TASK_FLAGS};
};

template<class T, Context ContextType>
struct ValueTaskPromise : TaskPromiseBase<T, ContextType> {
    template<class From>
        requires std::constructible_from<T, From&&>
    void yield_value(From &&from);
    template<class From>
        requires std::constructible_from<T, From&&>
    void return_value(From &&from);
};

template<Context ContextType>
struct VoidTaskPromise : TaskPromiseBase<void, ContextType> {
    void return_void();
};

template<class T, class ContextType>
struct TaskAwaiter {
    [[nodiscard]] bool await_ready() const noexcept;
    template<class InnerPromiseType>
    auto await_suspend(std::coroutine_handle<InnerPromiseType> handle) noexcept -> std::coroutine_handle<>;
    auto await_resume();

    std::coroutine_handle<typename Task<T, ContextType>::promise_type> _handle;
};

}

template<class T, Context ContextType>
class Task {
    void destroy() noexcept;
public:
    using promise_type = std::conditional_t<
        std::is_void_v<T>,
        detail::VoidTaskPromise<ContextType>,
        detail::ValueTaskPromise<T, ContextType>
    >;

    Task() noexcept = default;
    explicit Task(std::coroutine_handle<promise_type> handle) noexcept;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept;
    auto operator=(Task&& other) noexcept -> Task&;

    ~Task() noexcept { destroy(); }

    auto get_async() const noexcept;
    auto operator co_await() const & noexcept { return get_async(); }
    auto operator co_await() const && noexcept { return get_async(); }

    explicit operator bool() const noexcept { return static_cast<bool>(_handle); }
    void start() noexcept;
    void cancel() noexcept;

    // allow a task to resume in the background and clean itself up later (nice for fire-and-forget tasks)
    // type-erased to allow std::noop_coroutine() -> never returns an invalid handle
    auto detach() noexcept -> std::coroutine_handle<>;
    [[nodiscard]] bool is_detached() const noexcept;
private:
    std::coroutine_handle<promise_type> _handle;
};

// @TODO implementation header?

template <class T, Context ContextType> void Task<T, ContextType>::destroy() noexcept {
    if (!_handle || (_handle.promise().is_detached())) {
        return;
    }

    _handle.destroy();
    _handle = {};
}

template<class T, Context ContextType>
Task<T, ContextType>::Task(std::coroutine_handle<promise_type> handle) noexcept
    : _handle{handle}
{}

template<class T, Context ContextType>
Task<T, ContextType>::Task(Task &&other) noexcept
    : _handle{ std::exchange(other._handle, {}) }
{}

template<class T, Context ContextType>
auto Task<T, ContextType>::operator=(Task &&other) noexcept -> Task& {
    if (this != &other) {
        destroy();
        _handle = std::exchange(other._handle, {});
    }
    return *this;
}

template<class T, Context ContextType>
auto Task<T, ContextType>::get_async() const noexcept {
    return detail::TaskAwaiter<T, ContextType>{ _handle };
}

template<class T, Context ContextType>
void Task<T, ContextType>::start() noexcept {
    if (_handle) {
        auto &p = _handle.promise();
        p.detach();
        _handle.resume();
    }
}

template <class T, Context ContextType> void Task<T, ContextType>::cancel() noexcept {
    if (_handle) {
        auto &p = _handle.promise();
        p._flags |= detail::TASK_CANCELLED;
    }
}

template <class T, Context ContextType>
auto Task<T, ContextType>::detach() noexcept-> std::coroutine_handle<> {
    if (!_handle) {
        return std::noop_coroutine();
    }

    auto handle = std::exchange(_handle, {}); // give up ownership now
    handle.promise().detach();
    return handle;
}

template <class T, Context ContextType> bool Task<T, ContextType>::is_detached() const noexcept {
    if (!_handle) {
        return false;
    }

    return _handle.promise()._flags & detail::TASK_DETACHED;
}

namespace detail {

template <class T, Context ContextType>
auto TaskPromiseBase<T, ContextType>::initial_suspend() const noexcept -> std::suspend_always {
    // tasks always start on the calling thread
    // rescheduling should be done by a specialized awaiter (-> asyncrt::core)
    return {};
}

template <class T, Context ContextType>
auto TaskPromiseBase<T, ContextType>::final_suspend() const noexcept{
    struct Awaiter {
        FORCE_SUSPEND

        std::coroutine_handle<> await_suspend(std::coroutine_handle<typename Task<T, ContextType>::promise_type> handle) noexcept {
            if (!handle) [[unlikely]] {
                return std::noop_coroutine();
            }

            auto& p = handle.promise();
            if (p._continuation) {
                if ((p._flags & TASK_RESUME_ON_CAPTURED_CONTEXT) && p._captured_ctx) {
                    // post the continuation on the captured context if that flag is set
                    p._captured_ctx->post(post_coroutine, p._continuation);
                    return std::noop_coroutine();
                }
                // otherwise, continue with the continuation
                return p._continuation;
            }

            // if we are detached and there is no continuation: destroy the frame by yourself
            if (p._flags & TASK_DETACHED)  {
                handle.destroy();
            }

            return std::noop_coroutine(); // return a noop continuation
        }

        void await_resume() const noexcept {}
    };
    return Awaiter{};
}

template <class T, Context ContextType>
Task<T, ContextType> TaskPromiseBase<T, ContextType>::get_return_object(this auto &self) noexcept {
    return Task<T, ContextType>{ std::coroutine_handle<typename Task<T, ContextType>::promise_type>::from_promise(self) };
}

template<class T, Context ContextType>
void TaskPromiseBase<T, ContextType>::unhandled_exception() {
    _state.set_exception(std::current_exception());
}

template <class T, Context ContextType> bool TaskPromiseBase<T, ContextType>::is_detached() const noexcept{
    return _flags & TASK_DETACHED;
}

template <class T, Context ContextType> void TaskPromiseBase<T, ContextType>::detach() noexcept {
    _flags |= TASK_DETACHED;
}

template <class T, Context ContextType>
template <class From>
    requires std::constructible_from<T, From &&>
void ValueTaskPromise<T, ContextType>::yield_value(From &&from) {
    this->_state.set_value(std::forward<From>(from));
}

template <class T, Context ContextType>
template <class From>
    requires std::constructible_from<T, From &&>
void ValueTaskPromise<T, ContextType>::return_value(From &&from) {
    this->_state.set_value(std::forward<From>(from));
}

template<Context ContextType>
void VoidTaskPromise<ContextType>::return_void() {
    this->_state.set_value();
}

template<class T, class ContextType>
bool TaskAwaiter<T, ContextType>::await_ready() const noexcept {
    return !_handle || _handle.done(); // shortcut either if no frame is awaited or the awaited frame is already done
}

template<class T, class ContextType>
template<class InnerPromiseType>
auto TaskAwaiter<T, ContextType>::await_suspend(std::coroutine_handle<InnerPromiseType> handle) noexcept -> std::coroutine_handle<>{
    if (!handle) [[unlikely]] {
        return std::noop_coroutine();
    }

    auto &p = _handle.promise();
    p._continuation = handle;
    // until the first suspension, the task owns itself. after that, it gets detached
    // p.detach();
    // capture the context
    // if we manually set a context already, keep it instead
    // implementations need to handle all the thread-locality stuff like selecting a proper context type
    if (!p._captured_ctx) {
        p._captured_ctx = std::make_unique<ContextType>();
    }

    return _handle;
}

template<class T, class ContextType>
auto TaskAwaiter<T, ContextType>::await_resume(){
    try {
        auto future = _handle.promise()._state.get_future();
        if constexpr (std::is_same_v<T, void>) {
            future.get();
        } else {
            return std::move(future.get());
        }
    } catch (...) {
        std::rethrow_exception(std::current_exception());
    }
}



}

}