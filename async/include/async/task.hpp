#pragma once
#include <coroutine>
#include <exception>
#include <future>

#include "context.hpp"

namespace async {
namespace detail {

struct InitiallyScheduleOnCurrentContext; // InitiallyScheduleOnCurrentContext schedules a task immediately on the current context

}

template<class T, bool IsScheduled = true, bool IsDeferred_ = false>
struct DefaultTaskLaunchPolicy {
    using InitialSuspendType = T;
    static constexpr bool IsDeferred{ IsDeferred_ };

    template<class P>
    static void Launch(std::coroutine_handle<P> handle) {
        if (!handle) {
            return;
        }

        if constexpr (IsScheduled) {
            const auto ctx = SynchronizationContext::Current();
            if (!ctx) {
                throw SynchronizationContextUnavailable{};
            }
            ctx->Post(handle);
        } else {
            handle.resume();
        }
    }
};

struct ManualLaunchSchedulePolicy {
    using InitialSuspendType = std::suspend_always;
    static constexpr bool IsDeferred{ false };

    // launch and schedule as two separate functions

    template<class P>
    static void Launch(std::coroutine_handle<P> handle) {
        if (!handle) {
            return;
        }
        handle.resume();
    }

    template<class P>
    static void Schedule(std::coroutine_handle<P> handle) {
        if (!handle) {
            return;
        }

        const auto ctx = SynchronizationContext::Current();
        if (!ctx) {
            throw SynchronizationContextUnavailable{};
        }
        ctx->Post(handle);
    }
};

using Manual = ManualLaunchSchedulePolicy; // task must be manually launched / scheduled

using LaunchDeferred = DefaultTaskLaunchPolicy<std::suspend_always, false, true>; // task will be launched on scope exit
using LaunchImmediately = DefaultTaskLaunchPolicy<std::suspend_never, false>; // task is immediately launched on the current thread

using ScheduleDeferred = DefaultTaskLaunchPolicy<std::suspend_always, true, true>; // task will be scheduled on the current context on scope exit
using ScheduleImmediately = DefaultTaskLaunchPolicy<detail::InitiallyScheduleOnCurrentContext>; // task is immediately scheduled on the current context

template<class T, class LaunchPolicy = Manual>
class Task;

namespace detail {
// initial awaiter which schedules the coroutine on the correct context
struct InitiallyScheduleOnCurrentContext {
    bool await_ready() const noexcept;

    template<class PromiseType>
    void await_suspend(std::coroutine_handle<PromiseType> handle) {
        if (!handle) {
            throw std::invalid_argument{ "Invalid coroutine handle" };
        }

        auto& promise = handle.promise();

        // As soon as the coroutine starts, schedule it on the synchronization context and detach it. If no synchronization context is available, continue it as an "owned" task
        if (const auto ctx = SynchronizationContext::Current();
            ctx && !promise._detached) {
            promise._detached = true;
            ctx->Post(handle);
        } else {
            throw SynchronizationContextUnavailable{};
        }
    }

    void await_resume() const noexcept;
};

template<class T, class LaunchPolicy>
struct Promise {
    using InitialAwaiter = LaunchPolicy::InitialSuspendType;

    // final awaiter which handles continuation handover or cleanup of a detached coroutine
    struct FinalAwaiter {
        bool await_ready() const noexcept { return false; }

        template<class PromiseType>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
            auto& p = handle.promise();
            if (p._continuation) {
                return p._continuation;
            }

            // self-managed cleanup in the final suspension if we are the last frame
            if (p._detached)  {
                handle.destroy();
            }

            return std::noop_coroutine(); // return a noop continuation
        }

        void await_resume() const noexcept {}
    };

    InitialAwaiter initial_suspend() const noexcept { return InitialAwaiter{}; }
    FinalAwaiter final_suspend() const noexcept { return FinalAwaiter{}; }
    void unhandled_exception() noexcept {
        _state.set_exception(std::current_exception());
    }

    template<class From>
        requires std::is_assignable_v<T, From&&>
    void yield_value(From &&value) { // no noexcept because of promise.set_value
        _state.set_value(T{std::forward<From>(value)});
    }

    void return_void() requires std::is_void_v<T> {
        _state.set_value();
    }

    Task<T, LaunchPolicy> get_return_object() noexcept {
        return Task<T, LaunchPolicy>{ std::coroutine_handle<Promise>::from_promise(*this)};
    }

    // internal state
    std::coroutine_handle<> _continuation;
    std::promise<T> _state;
    bool _detached{ false };
};

template<class PromiseType, class LaunchPolicy = Manual>
struct TaskAwaiter {
    [[nodiscard]] bool await_ready() const noexcept {
        return !_handle || _handle.done(); // shortcut either if no frame is awaited or the awaited frame is done
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> handle) noexcept {
        if (!handle) {
            throw std::invalid_argument{ "Invalid coroutine handle" };
        }

        _handle.promise()._continuation = handle; // the awaited handle is a continuation of the new handle
        return _handle;
    }

    auto await_resume() {
        try {
            auto future = _handle.promise()._state.get_future();
            if constexpr (std::is_same_v<PromiseType, Promise<void, LaunchPolicy>>) {
                future.get();
            } else {
                return std::move(future.get());
            }
        } catch (...) {
            std::rethrow_exception(std::current_exception());
        }
    }

    std::coroutine_handle<PromiseType> _handle;
};

}

template<class T, class LaunchPolicy>
class Task {
    void Destroy() {
        if (!_handle || _handle.promise()._detached) {
            return;
        }

        _handle.destroy();
        _handle = {};
    }

    friend class SynchronizationContext;
public:
    // coroutine traits
    using promise_type = detail::Promise<T, LaunchPolicy>;

    using TaskHandle = std::coroutine_handle<promise_type>;

    Task() noexcept = default;
    explicit Task(TaskHandle handle) noexcept
        : _handle{handle}
    {}

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept
        : _handle{ std::exchange(other._handle, {}) }
    {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            Destroy();
            _handle = std::exchange(other._handle, {});
        }
        return *this;
    }

    ~Task() {
        if constexpr(LaunchPolicy::IsDeferred) {
            LaunchPolicy::Launch(Detach());
        }
        Destroy();
    }

    detail::TaskAwaiter<promise_type, LaunchPolicy> operator co_await() & noexcept {
        return { _handle };
    }

    detail::TaskAwaiter<promise_type, LaunchPolicy> operator co_await() && noexcept {
        return { _handle };
    }

    // helper to manually invoke task asynchronously on the current context
    // can only be used if manual launch is configured
    void InvokeAsync() requires std::same_as<LaunchPolicy, Manual> {
        Manual::Schedule(Detach());
    }

    // helper to manually invoke task synchronously
    // can only be used if manual launch is configured
    void Invoke() requires std::same_as<LaunchPolicy, Manual> {
        Manual::Launch(_handle);
    }

    std::coroutine_handle<> Detach() {
        if (!_handle) {
            // no owned frame, no-op
            return std::noop_coroutine();
        }

        auto& promise = _handle.promise();
        if (promise._detached) {
            // already detached, no-op
            return std::noop_coroutine();
        }

        promise._detached = true;
        return std::exchange(_handle, {});
    }
private:
    TaskHandle _handle;
};
}