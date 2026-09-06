#pragma once
#include <memory>
#include <asyncrt/core/context.hpp>
#include <asyncrt/core/resume_on.hpp>

#include <QObject>

namespace asyncrt::qt {

class Dispatcher;
class Context {
public:
    // construct and select a fitting dispatcher
    Context();
    explicit Context(const std::shared_ptr<Dispatcher> &dispatcher);

    bool post(core::PostCoroutineTag, std::coroutine_handle<> handle);
    bool post(std::move_only_function<void()> callable);

    void stop();
private:
    std::shared_ptr<Dispatcher> _dispatcher{nullptr};
};

auto resume_on_threadpool() -> core::ResumeOnAwaitable<Context>;

}