#include <asyncrt/qt/context.hpp>
#include <asyncrt/qt/dispatcher.hpp>

#include <QCoreApplication>

namespace asyncrt::qt {

// get the default dispatcher for the calling thread
// if the current thread is the main application thread, the returned dispatcher is attached to QCoreApplication
// otherwise, a threadpool dispatcher pointing to the global thread pool is returned and is a child of the associated context (will be cleaned up automatically)
static auto default_dispatcher() -> Dispatcher* {
    if (QThread::isMainThread()) {
        return EventLoopDispatcher::application_dispatcher();
    }
    return new ThreadPoolDispatcher{};
}

static void destroy_dispatcher(const Dispatcher *dispatcher) {
    if (const auto event_loop_dispatcher = dynamic_cast<const EventLoopDispatcher *>(dispatcher);
        event_loop_dispatcher && event_loop_dispatcher->parent()) {
        // do not delete the event loop dispatcher if it has a parent
        // if it does, the memory cleanup will be done by the Qt object tree
        // this case is very likely to happen if any context is requested on the main application thread
        return;
    }

    delete dispatcher;
}

Context::Context()
    : Context(std::shared_ptr<Dispatcher>(default_dispatcher(), &destroy_dispatcher))
{}

Context::Context(const std::shared_ptr<Dispatcher> &dispatcher)
    : _dispatcher{dispatcher} {
}

bool Context::post(core::PostCoroutineTag, std::coroutine_handle<> handle) {
    return _dispatcher->post_coroutine(handle);
}

bool Context::post(std::move_only_function<void()> callable) {
    return _dispatcher->post_delegate(std::forward<decltype(callable)>(callable));
}

void Context::stop() {
    _dispatcher->stop();
}

auto resume_on_threadpool() -> core::ResumeOnAwaitable<Context> {
    return core::resume_on(Context{std::make_shared<ThreadPoolDispatcher>()});
}

auto resume_on_application_thread() -> core::ResumeOnAwaitable<Context> {
    return core::resume_on(Context{std::shared_ptr<EventLoopDispatcher>(EventLoopDispatcher::application_dispatcher(), &destroy_dispatcher)});
}

}