#pragma once
#include <future>


#include <async/context.hpp>
#include <async/dispatcher.hpp>

template<class Fn, bool IsCancellable>
struct RunOnWorkerResult;

template<class Fn>
struct RunOnWorkerResult<Fn, true> {
    using type = std::invoke_result_t<std::decay_t<Fn>, std::stop_token>;
};

template<class Fn>
struct RunOnWorkerResult<Fn, false> {
    using type = std::invoke_result_t<std::decay_t<Fn>>;
};

template<class Fn>
    requires std::invocable<Fn> || std::invocable<Fn, std::stop_token>
class RunOnWorkerAwaitable {
    static constexpr bool IsCooperativelyCancellable{std::is_invocable_v<Fn, std::stop_token>};
    using Result = RunOnWorkerResult<Fn, IsCooperativelyCancellable>::type;

    class WorkerContinuationOperation : public async::IDispatcherOperation {
    public:
        WorkerContinuationOperation(RunOnWorkerAwaitable& awaitable, std::shared_ptr<async::SynchronizationContext> ctx, std::coroutine_handle<> handle)
            : _awaitable{awaitable}
            , _ctx{std::move(ctx)}
            , _handle{handle} {
        }

        ~WorkerContinuationOperation() override {
            if (_handle) {
                _handle.destroy();
            }
        }

        void Invoke() override {
            // observe a handle for completion, re-post it on the context if it didnt complete yet
            if (!_handle) {
                return;
            }

            auto handle = std::exchange(_handle, {});
            if (!_awaitable.await_ready()) {
                // do another round-trip and check if the awaitable is ready
                _ctx->Post(std::make_unique<WorkerContinuationOperation>(_awaitable, _ctx, handle));
                return;
            }

            // if the worker is done but the handle was killed (by Cancel() or some other source)
            // we destroy the handle. this avoids blocking the current thread when cancelling and
            // essentially corresponds to "as soon as the awaitable is ready, delete it (if killed)"

            // for the user this translates to the the coroutine basically "aborting" after the particular co_await has finished
            if (_awaitable._killed) {
                handle.destroy();
                return;
            }

            // finally, if nothing else happened we can resume
            handle.resume();
        }

    private:
        RunOnWorkerAwaitable& _awaitable;
        std::shared_ptr<async::SynchronizationContext> _ctx;
        std::coroutine_handle<> _handle;
    };

    auto DoInvoke(std::stop_token st) {
        if constexpr (IsCooperativelyCancellable) {
            return _fn(st);
        } else {
            return _fn();
        }
    }

    void ThreadFn(std::stop_token st) {
        try {
            if constexpr (std::is_void_v<Result>) {
                DoInvoke(st);
                _promise.set_value();
            } else {
                _promise.set_value(DoInvoke(st));
            }
        } catch (...) {
            _promise.set_exception(std::current_exception());
        }
    }
public:
    RunOnWorkerAwaitable(Fn fn, std::stop_token externalStopToken)
        : _fn(fn)
        , _promise{}
        , _future(_promise.get_future())
        , _stopToken(std::move(externalStopToken)) // clone the stop token provided so it is shared
        , _stopCallback(_stopToken, [this] {
            _killed = true;
        }) {
        // the external stop token can stop and already requested to stop
        if (_stopToken.stop_possible() && _stopToken.stop_requested()) {
            _killed = true;
        }
    }

    [[nodiscard]] bool await_ready() const noexcept {
        // if the result already completed, we can shortcut
        // we do NOT take _killed into account; otherwise the awaitable would short-circuit and still block when retrieving the future value
        return std::future_status::ready == _future.wait_for(std::chrono::milliseconds{ 0 });
    }

    void await_suspend(std::coroutine_handle<> handle) {
        // the task was cancelled before it even started
        if (_killed) {
            return; // safety measure, should never arrive here
        }

        auto ctx = async::SynchronizationContext::Current();
        if (!ctx) {
            throw async::SynchronizationContextUnavailable{};
        }

        _worker = std::jthread{ [this] {
            ThreadFn(_stopToken);
        }};
        // if not externally provided (-> no state, !stop_possible()), set the workers stop token
        if (!_stopToken.stop_possible()) {
            _stopToken = _worker.get_stop_token();
        }
        ctx->Post(std::make_unique<WorkerContinuationOperation>(*this, ctx, handle));
    }

    auto await_resume() {
        if constexpr (std::is_void_v<Result>) {
            _future.get();
        } else {
            return std::move(_future.get());
        }
    }

    void Cancel() {
        if (!_stopToken.stop_possible()) {
            _worker.request_stop(); // will also flip _stopToken if internally managed
        }
        _killed = true;
    }
private:
    Fn _fn;

    std::promise<Result> _promise;
    std::future<Result> _future;
    std::jthread _worker;
    std::stop_token _stopToken; // own stop token in case an external (cloned) token is used
    std::stop_callback<std::move_only_function<void()>> _stopCallback; // stop callback installed on start in case the stop token cancels early
    std::atomic_bool _killed{ false }; // "killed" flag in case a task is cancelled before it is even started or the cancellation token can not be set to cancelled
};

// helper to run non-async potentially blocking functions on a worker thread
// optionally provide an external stop token, empty by default (will fall back to internal one on await)
template<class Fn>
auto RunOnWorker(Fn fn, std::stop_token st = std::stop_token{}) {
    return RunOnWorkerAwaitable<Fn>{ fn, st };
}

// @TODO MFC Module

#define DECLARE_ASYNC_HANDLER(Name, ...) \
    afx_msg void Name(); \
    async::Task<void> Name##_Async();

#define IMPLEMENT_ASYNC_HANDLER(ClassName, Name, ...) \
void ClassName::Name() { Name##_Async().InvokeAsync(); } \
async::Task<void> ClassName::Name##_Async()
