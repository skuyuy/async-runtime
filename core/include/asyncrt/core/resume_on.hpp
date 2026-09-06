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
        // handle.promise().detach();
        // post the handle, detach it and immediately return to the caller
        ctx.post(post_coroutine, handle);
    }

    void await_resume() const noexcept { /*ignore*/ }

    ContextType ctx;
};

auto resume_on(Context auto ctx = {}) -> ResumeOnAwaitable<std::decay_t<decltype(ctx)>> {
    return ResumeOnAwaitable<std::decay_t<decltype(ctx)>>{ctx};
}

}