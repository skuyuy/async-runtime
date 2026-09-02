#include <async/dispatcher.hpp>

namespace async {

std::size_t Dispatcher::RemainingOperations() const {
    std::unique_lock lock{ _mtx };
    return _operations.size();
}

void Dispatcher::Push(std::unique_ptr<IDispatcherOperation> operation) {
    if (!operation) {
        throw std::invalid_argument("Invalid operation");
    }

    std::unique_lock lock{ _mtx };
    _operations.push(std::move(operation));
}

std::unique_ptr<IDispatcherOperation> Dispatcher::PullNextOperation() {
    std::unique_lock lock{ _mtx };
    if (_operations.empty()) {
        return nullptr;
    }

    auto op = std::move(_operations.front());
    _operations.pop();
    return op;
}

void Dispatcher::ProcessNextOperations() {
    // process all items currently in the queue
    auto remainingOperations = RemainingOperations();
    while (remainingOperations-- > 0) {
        if (const auto op = PullNextOperation(); op) {
            op->Invoke();
        }
    }
}

void Dispatcher::Clear() {
    std::unique_lock lock{ _mtx };
    while (!_operations.empty()) {
        const auto op = std::move(_operations.front());
        _operations.pop();
    }
}

}
