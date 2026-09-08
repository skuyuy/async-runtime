#pragma once
#include <exception>
#include <future>
#include <concepts>
#include <coroutine>
#include <expected>
#include <utility>
#include <stop_token>

#include "detail/coro_utils.hpp"
#include "context.hpp"
#include "task_error.hpp"
#include "task_flags.hpp"
#include "task_traits.hpp"

namespace asyncrt::core {

template<class T, Context ContextType>
class Task;

namespace detail {

template<class T, Context ContextType>
struct FinalAwaiter {
    FORCE_SUSPEND
    NOOP_RESUME

    auto await_suspend(std::coroutine_handle<typename Task<T, ContextType>::promise_type> handle) noexcept -> std::coroutine_handle<>;
};

template<Context ContextType>
struct TaskPromiseBase {
    [[nodiscard]] auto initial_suspend() const noexcept -> std::suspend_always;

    void assign_stop_token(std::stop_token st);
    [[nodiscard]] bool is_cancelled() const noexcept;

    // cancellation from external sources (like std::jthread, std::stop_source)
    std::stop_token _st;
    std::optional<std::stop_callback<std::function<void()>>> _stop_callback;

    // internal promise state
    std::coroutine_handle<> _continuation{};
    std::optional<ContextType> _captured_ctx;
    std::atomic<std::underlying_type_t<TaskFlags>> _flags{DEFAULT_TASK_FLAGS}; // flags including detached state, internal cancellation state
    std::condition_variable_any _state_cv; // used to notify changes in coroutine state (currently only unfinished -> finished)
};

template<class T, Context ContextType>
struct ValueTaskPromise : TaskPromiseBase<ContextType> {
    auto get_return_object() noexcept -> Task<T, ContextType>;
    auto final_suspend() const noexcept -> FinalAwaiter<T, ContextType> { return {}; }
    void unhandled_exception();
    template<class From>
        requires std::constructible_from<T, From&&>
    void yield_value(From &&from);
    template<class From>
        requires std::constructible_from<T, From&&>
    void return_value(From &&from);

    auto get() -> T&&;
    void wait();

    std::promise<T> _state;
    std::future<T> _future{_state.get_future()};
};

template<Context ContextType>
struct VoidTaskPromise : TaskPromiseBase<ContextType> {
    auto get_return_object() noexcept -> Task<void, ContextType>;
    auto final_suspend() const noexcept -> FinalAwaiter<void, ContextType> { return {}; }
    void unhandled_exception();
    void return_void() noexcept;
    void get();
    void wait();

    std::atomic_bool _finished;
    std::exception_ptr _exception;
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

    auto unwrap();
    auto try_unwrap() -> std::expected<T, std::error_code>;
    void wait();

    auto get_async() const noexcept;
    auto operator co_await() const & noexcept { return get_async(); }
    auto operator co_await() const && noexcept { return get_async(); }

    explicit operator bool() const noexcept { return static_cast<bool>(_handle); }
    void start(std::stop_token st) noexcept;

    void cancel() noexcept;
    [[nodiscard]]
    bool is_canceled() noexcept;

    // allow a task to resume in the background and clean itself up later (nice for fire-and-forget tasks)
    // type-erased to allow std::noop_coroutine() -> never returns an invalid handle
    auto detach() noexcept -> std::coroutine_handle<>;
    [[nodiscard]]
    bool is_detached() const noexcept;
private:
    std::coroutine_handle<promise_type> _handle;
};

// @TODO implementation header?

template <class T, Context ContextType> void Task<T, ContextType>::destroy() noexcept {
    if (!_handle || (_handle.promise()._flags & TASK_DETACHED)) {
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

template <class T, Context ContextType>
auto Task<T, ContextType>::unwrap(){
    if (!_handle) {
        throw TaskException::invalid_handle();
    }

    return _handle.promise().get();
}

template <class T, Context ContextType>
auto Task<T, ContextType>::try_unwrap() -> std::expected<T, std::error_code> {
    if (!_handle) {
        return std::unexpected{TaskError::invalid_handle};
    }

    try {
        if constexpr (std::is_void_v<T>) {
            _handle.promise().get();
            return {};
        } else {
            return _handle.promise().get();
        }
    } catch (const TaskException &task_err) {
        return std::unexpected{task_err.code};
    } catch (const std::system_error &sys_err) {
        return std::unexpected{sys_err.code()};
    } catch (const std::exception &) {
        return std::unexpected{TaskError::exception};
    } catch (...) {
        return std::unexpected{TaskError::unknown};
    }
}

template <class T, Context ContextType>
void Task<T, ContextType>::wait(){
    assert(_handle);
    if (!_handle) {
        return; // warn??
    }

    _handle.promise().wait();
}

template<class T, Context ContextType>
auto Task<T, ContextType>::get_async() const noexcept {
    return detail::TaskAwaiter<T, ContextType>{ _handle };
}

template<class T, Context ContextType>
void Task<T, ContextType>::start(std::stop_token st) noexcept {
    if (_handle) {
        auto &p = _handle.promise();
        p.assign_stop_token(st);
        p._flags |= TASK_DETACHED;
        _handle.resume();
    }
}

template <class T, Context ContextType>
void Task<T, ContextType>::cancel() noexcept {
    if (_handle) {
        auto &p = _handle.promise();
        p._flags |= TASK_CANCELLED;
        p._state_cv.notify_all(); // notify potential waiters
    }
}

template <class T, Context ContextType> bool Task<T, ContextType>::is_canceled() noexcept{
    // released / null tasks are implicitly canceled
    return _handle ? _handle.promise().is_canceled() : true;
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

    return _handle.promise()._flags & TASK_DETACHED;
}

namespace detail {

template <class T, Context ContextType>
auto FinalAwaiter<T, ContextType>::await_suspend(std::coroutine_handle<typename Task<T, ContextType>::promise_type> handle) noexcept-> std::coroutine_handle<> {
    if (!handle) [[unlikely]] {
        return std::noop_coroutine();
    }

    auto& p = handle.promise();
    if (p.is_cancelled()) {
        return std::noop_coroutine();
    }

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

template <Context ContextType>
auto TaskPromiseBase<ContextType>::initial_suspend() const noexcept -> std::suspend_always {
    // tasks always start on the calling thread
    // rescheduling should be done by a specialized awaiter (-> asyncrt::core)
    return {};
}

template <class T, Context ContextType>
Task<T, ContextType> ValueTaskPromise<T, ContextType>::get_return_object() noexcept {
    return Task<T, ContextType>{ std::coroutine_handle<typename Task<T, ContextType>::promise_type>::from_promise(*this) };
}

template<class T, Context ContextType>
void ValueTaskPromise<T, ContextType>::unhandled_exception() {
    _state.set_exception(std::current_exception());
}

template <Context ContextType>
void TaskPromiseBase<ContextType>::assign_stop_token(std::stop_token st) {
    _st = std::move(st);
    _stop_callback.emplace(_st, [this] {
        _flags |= TASK_CANCELLED;
        _state_cv.notify_all();
    });
}

template <Context ContextType> bool TaskPromiseBase<ContextType>::is_cancelled() const noexcept {
    return (_flags & TASK_CANCELLED)
        || _st.stop_requested();
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

template <class T, Context ContextType>
auto ValueTaskPromise<T, ContextType>::get() -> T&& {
    if (this->is_canceled()) {
        throw TaskException::cancelled();
    }
    return std::move(_future.get());
}

template<class T, Context ContextType>
void ValueTaskPromise<T, ContextType>::wait() {
    if (this->is_canceled() || _future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        return;
    }

    // we cant just wait for the future, we also need to respect cancellation
    std::mutex mtx;
    std::unique_lock lock{mtx};
    // cancellation can happen either via stop token or other cancellation sources
    this->state_cv.wait(lock, this->_st, [this] {
        return _future.wait_for(std::chrono::seconds(0)) == std::future_status::ready
            || (this->_flags & TASK_CANCELLED);
    });
}

template <Context ContextType>
auto VoidTaskPromise<ContextType>::get_return_object() noexcept-> Task<void, ContextType> {
    return Task<void, ContextType>{ std::coroutine_handle<VoidTaskPromise>::from_promise(*this) };
}

template <Context ContextType>
void VoidTaskPromise<ContextType>::unhandled_exception() {
    _exception = std::current_exception();
    this->_state_cv.notify_all();
}

template <Context ContextType>
void VoidTaskPromise<ContextType>::return_void() noexcept {
    _finished = true;
    this->_state_cv.notify_all();
}

template <Context ContextType>
void VoidTaskPromise<ContextType>::get() {
    // if not yet finished / exception / canceled, wait here
    if (!(_finished || _exception || this->is_cancelled())) {
        std::mutex mtx;
        std::unique_lock lock{mtx};
        // wait for stop token or exception state
        this->_state_cv.wait(lock, this->_st, [this] {
            return _finished
                || _exception != nullptr
                || (this->_flags & TASK_CANCELLED);
        });
    }

    // handle cancel and exceptions
    if (this->is_cancelled()) {
        throw TaskException::cancelled();
    }

    if (_exception) {
        std::rethrow_exception(_exception);
    }
}

template <Context ContextType>
void VoidTaskPromise<ContextType>::wait() {
    // shortcut
    if (this->is_canceled() || _finished || _exception) {
        return;
    }

    std::mutex mtx;
    std::unique_lock lock{mtx};
    // wait for finished or exception state or stop token
    // waiting with stop token is fine here since a task, when cancelled, can return immediately from wait()
    this->_state_cv.wait(lock, this->_st, [this] {
        return _finished
            || _exception != nullptr
            || (this->_flags & TASK_CANCELLED);
    });
}

template<class T, class ContextType>
bool TaskAwaiter<T, ContextType>::await_ready() const noexcept {
    if (!_handle) {
        return true;
    }

    // shortcut either if no frame is awaited or the awaited frame is already done or the task is cancelled
    return _handle.done()
           || _handle.promise().is_canceled();
}

template<class T, class ContextType>
template<class InnerPromiseType>
auto TaskAwaiter<T, ContextType>::await_suspend(std::coroutine_handle<InnerPromiseType> handle) noexcept -> std::coroutine_handle<> {
    if (!handle) [[unlikely]] {
        return std::noop_coroutine();
    }

    auto &p = _handle.promise();
    // no need to check for cancellation here since in await_ready, we take the shortcut if the task has been cancelled

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
    return _handle.promise().get();
}

}

}