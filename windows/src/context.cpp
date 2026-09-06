#include <asyncrt/windows/context.hpp>
#include <asyncrt//windows/dispatcher.hpp>

namespace asyncrt::windows {

Context::Context()
    : Context(Dispatcher::current()) // use the current dispatcher (artifact from old C#-ish API)
{}

Context::Context(const std::shared_ptr<Dispatcher> &dispatcher) {
    if (!dispatcher) {
        throw std::invalid_argument{"Invalid dispatcher for context"};
    }
    _dispatcher = dispatcher;
}

bool Context::post(core::PostCoroutineTag, std::coroutine_handle<> handle) {
    if (_dispatcher.expired()) {
        handle.destroy();
        return false; // dispatcher was destroyed so we should clean up the handle
    }

    const auto dispatcher = _dispatcher.lock();
    // if the dispatcher of this context is the same as the current one, we can just immediately resume and save some overhead
    /*if (dispatcher == Dispatcher::current() && !dispatcher->is_stopped()) {
        handle.resume();
        return true;
    }*/

    return dispatcher->submit(DispatcherItem{handle});
}

bool Context::post(std::move_only_function<void()> callable) {
    if (_dispatcher.expired()) {
        return false; // dispatcher was destroyed
    }

    const auto dispatcher = _dispatcher.lock();
    // if the dispatcher of this context is the same as the current one, invoke here
    if (dispatcher == Dispatcher::current()) {
        callable();
        return true;
    }

    return dispatcher->submit(DispatcherItem{std::move(callable)});
}

void Context::stop() {
    if (_dispatcher.expired()) {
        return;
    }
    _dispatcher.lock()->stop();
}

auto resume_on_threadpool() -> core::ResumeOnAwaitable<Context> {
    return core::resume_on<Context>(Context{ThreadPoolDispatcher::instance()});
}

}