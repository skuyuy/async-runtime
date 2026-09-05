#pragma once
#include "context.hpp"
#include "detail/coro_utils.hpp"
#include "task.hpp"

namespace asyncrt::core {

template<Context ContextType>
struct ResumeOnAwaitable {
    FORCE_SUSPEND

    template<class T>
    void await_suspend(std::coroutine_handle<T> handle) const {
        if (!context || context == ContextType::current()) {
            // resume here if the context is not available or the same as the current context
            handle.resume();
        }

        handle.promise()._flags |= detail::TASK_DETACHED;
        // post the handle, detach it and immediately return to the caller
        context->post(post_coroutine, handle);
    }

    void await_resume() const noexcept { /*ignore*/ }

    std::shared_ptr<ContextType> context;
};

template<Context ContextType>
auto resume_on(const std::shared_ptr<ContextType> &context) -> ResumeOnAwaitable<ContextType> {
    return { context };
}

}