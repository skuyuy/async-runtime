#pragma once
#include <concepts>
#include <memory>
#include <mutex>
#include <coroutine>
#include <functional>

#include "dispatcher.hpp"

namespace async {

struct SynchronizationContextUnavailable : std::runtime_error {
    std::thread::id tid; // Thread for which no context could be provided

    explicit SynchronizationContextUnavailable(std::thread::id tid = std::this_thread::get_id());
};

class SynchronizationContext
{
public:
    virtual ~SynchronizationContext() = default;

    virtual void Send(std::unique_ptr<IDispatcherOperation> operation) = 0;
    virtual void Post(std::unique_ptr<IDispatcherOperation> operation) = 0;
    virtual void Process() = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Shutdown() = 0;

    template<class... Args>
    void Send(std::invocable<Args...> auto fn, Args&&... args);
    template<class... Args>
    void Post(std::invocable<Args...> auto fn, Args&&... args);

    template<class Promise>
    void Send(std::coroutine_handle<Promise> handle);
    template<class Promise>
    void Post(std::coroutine_handle<Promise> handle);

    static std::shared_ptr<SynchronizationContext> Current();
    static void SetCurrent(const std::shared_ptr<SynchronizationContext> &context);
};

// --- Implementations

template<class... Args>
void SynchronizationContext::Send(std::invocable<Args...> auto fn, Args&&... args) {
    Send(std::make_unique<InvokableDispatcherOperation>(std::bind_front(fn, std::forward<Args>(args)...)));
}

template<class... Args>
void SynchronizationContext::Post(std::invocable<Args...> auto fn, Args&&... args) {
    Post(std::make_unique<InvokableDispatcherOperation>(std::bind_front(fn, std::forward<Args>(args)...)));
}

template<class Promise>
void SynchronizationContext::Send(std::coroutine_handle<Promise> handle) {
    Send(std::make_unique<CoroutineDispatcherOperation<Promise>>(handle));
}

template<class Promise>
void SynchronizationContext::Post(std::coroutine_handle<Promise> handle) {
    Post(std::make_unique<CoroutineDispatcherOperation<Promise>>(handle));
}

} // namespace async
