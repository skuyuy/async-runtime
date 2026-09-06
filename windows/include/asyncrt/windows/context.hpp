#pragma once
#include <memory>
#include <asyncrt/core/context.hpp>
#include <asyncrt/core/resume_on.hpp>

namespace asyncrt::windows {

class Dispatcher;
class Context {
public:
    // construct and select a fitting dispatcher
    Context();
    explicit Context(const std::shared_ptr<Dispatcher> &dispatcher);
    Context(std::nullptr_t) = delete;

    bool post(core::PostCoroutineTag, std::coroutine_handle<> handle);
    bool post(std::move_only_function<void()> callable);

    void stop();

private:
    std::weak_ptr<Dispatcher> _dispatcher; // the dispatcher to use for this context
};

auto resume_on_threadpool() -> core::ResumeOnAwaitable<Context>;

}