#pragma once
#include "context.hpp"
#include "detail/coro_utils.hpp"
#include "task.hpp"

namespace asyncrt::core {

template<Context ContextType>
struct ResumeOnAwaitable {
    FORCE_SUSPEND

    template<class T>
    void await_suspend(std::coroutine_handle<T> handle) {
        // if we are working with a task promise and it is cancelled, dont reschedule the coroutine
        if constexpr (std::is_base_of_v<detail::TaskPromiseBase<ContextType>, T>) {
            // if the task is cancelled, dont continue
            if (handle.promise().is_cancelled()) {
                return;
            }
        }

        ctx.post(post_coroutine, handle);
    }

    void await_resume() const noexcept { /*ignore*/ }

    ContextType ctx;
};

auto resume_on(Context auto ctx = {}) -> ResumeOnAwaitable<std::decay_t<decltype(ctx)>> {
    return ResumeOnAwaitable<std::decay_t<decltype(ctx)>>{ctx};
}

}