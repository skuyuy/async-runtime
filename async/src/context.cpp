#include <async/context.hpp>

#include <format>

namespace async {

thread_local std::shared_ptr<SynchronizationContext> threadLocalSynchronizationContext;

SynchronizationContextUnavailable::SynchronizationContextUnavailable(const std::thread::id tid)
    : std::runtime_error(std::format("SynchronizationContext unavailable for thread[#{}]", tid))
    , tid{tid}
{}

std::shared_ptr<SynchronizationContext> SynchronizationContext::Current() {
    // @TODO create dummy / default sync context and create it for thread local context
    return threadLocalSynchronizationContext;
}

void SynchronizationContext::SetCurrent(const std::shared_ptr<SynchronizationContext> &context) {
    if (!context) {
        throw std::invalid_argument("Invalid synchronization context");
    }

    threadLocalSynchronizationContext = context;
}

}