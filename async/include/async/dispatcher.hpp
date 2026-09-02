#pragma once
#include <queue>
#include <coroutine>
#include <mutex>

namespace async {

class IDispatcherOperation {
public:
    virtual ~IDispatcherOperation() = default;
    virtual void Invoke() = 0;
};

class Dispatcher {
    std::size_t RemainingOperations() const;
    std::unique_ptr<IDispatcherOperation> PullNextOperation();
public:
    void Push(std::unique_ptr<IDispatcherOperation> operation);
    void ProcessNextOperations();
    void Clear();
private:
    mutable std::mutex _mtx;
    std::queue<std::unique_ptr<IDispatcherOperation>> _operations;
};

template<std::invocable Fn>
class InvokableDispatcherOperation : public IDispatcherOperation {
public:
    explicit InvokableDispatcherOperation(Fn fn);
    void Invoke() noexcept(std::is_nothrow_invocable_v<Fn>) override;
private:
    Fn _fn;
    bool _invoked{false};
};

template<class Promise>
class CoroutineDispatcherOperation : public IDispatcherOperation {
public:
    explicit CoroutineDispatcherOperation(std::coroutine_handle<Promise> handle);
    ~CoroutineDispatcherOperation() override;

    void Invoke() override;
private:
    std::coroutine_handle<Promise> _handle;
};

// --- Implementations

template<std::invocable Fn>
InvokableDispatcherOperation<Fn>::InvokableDispatcherOperation(Fn fn): _fn{fn} {
}

template<std::invocable Fn>
void InvokableDispatcherOperation<Fn>::Invoke() noexcept(std::is_nothrow_invocable_v<Fn>) {
    if (_invoked) {
        return;
    }
    _invoked = true;
    _fn();
}

template<class Promise>
CoroutineDispatcherOperation<Promise>::CoroutineDispatcherOperation(std::coroutine_handle<Promise> handle) {
    if (!handle) {
        throw std::invalid_argument{"Invalid coroutine handle"};
    }
    _handle = handle;
}

template<class Promise>
CoroutineDispatcherOperation<Promise>::~CoroutineDispatcherOperation() {
    if (_handle) {
        std::exchange(_handle, {}).destroy();
    }
}

template <class Promise>
void CoroutineDispatcherOperation<Promise>::Invoke() {
    if (!_handle) {
        return;
    }
    std::exchange(_handle, {}).resume();
}

}