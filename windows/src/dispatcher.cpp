#include <cassert>
#include <stdexcept>
#include <future>

#include <asyncrt/windows/dispatcher.hpp>
#include <tchar.h>

#define DISPATCHER_CLASS_NAME _T("asyncrt.windows.message_thread_dispatcher")

namespace asyncrt::windows {

namespace tls {

// any thread-local dispatcher, open to custom implementations
thread_local std::shared_ptr<Dispatcher> local_dispatcher;

}

bool Dispatcher::send(core::PostCoroutineTag, std::coroutine_handle<> handle) {
    std::promise<bool> sig;

    auto invoke = [&sig, handle] {
        handle.resume();
        sig.set_value(true);
    };

    auto reject = [&sig, handle] {
        handle.destroy();
        sig.set_value(false);
    };

    if (submit(DispatcherItem{invoke, reject})) {
        return sig.get_future().get();
    }
    return false;
}

bool Dispatcher::send(std::move_only_function<void()> callable) {
    std::promise<bool> sig;

    auto invoke = [&sig, callable = std::move(callable)] mutable {
        callable();
        sig.set_value(true);
    };
    auto reject = [&sig] {
        sig.set_value(false);
    };

    if (submit(DispatcherItem{std::move(invoke), reject})) {
        return sig.get_future().get();
    }
    return false;
}

bool Dispatcher::post(core::PostCoroutineTag, std::coroutine_handle<> handle) {
    return submit(make_dispatcher_item(handle));
}

bool Dispatcher::post(std::move_only_function<void()> callable) {
    return submit(DispatcherItem{ .on_invoke = std::move(callable) });
}

std::shared_ptr<Dispatcher> Dispatcher::current() {
    // using a global thread-pool as a fallback, if no local dispatcher is configured
    static std::shared_ptr<ThreadPoolDispatcher> threadpool_dispatcher;

    if(!tls::local_dispatcher) {
        return ThreadPoolDispatcher::instance();
    }
    return tls::local_dispatcher;
}

void Dispatcher::set_current(const std::shared_ptr<Dispatcher> &dispatcher){
    tls::local_dispatcher = dispatcher;
}

void DispatcherItem::invoke() {
    assert(on_invoke);
    if (on_invoke) {
        on_invoke();
    }
}

void DispatcherItem::reject() {
    assert(on_reject);
    if (on_reject) {
        on_reject();
    }
}

auto make_noop_dispatcher_item()-> DispatcherItem {
    return {
        .on_invoke = []{},
        .on_reject = []{},
    };
}

auto make_dispatcher_item(std::coroutine_handle<> handle)-> DispatcherItem {
    assert(handle);
    if (!handle) {
        return make_noop_dispatcher_item();
    }

    // if the dispatcher rejects the handle, it must be cleaned up
    // example scenario: stopping the runtime while some tasks are still in-flight
    // if the runtime is stopped, it rejects the submitted items
    // in order for the handles to not leak, we need to destroy them if they are rejected
    return {
        .on_invoke = [handle] { handle.resume(); },
        .on_reject = [handle] { handle.destroy(); }
    };
}

void Dispatcher::stop() noexcept {
    _stopped = true;
}

bool Dispatcher::is_stopped() const noexcept {
    return _stopped;
}

bool MessageThreadDispatcher::submit(DispatcherItem &&item) {
    if (is_stopped()) {
        // exit scope here
        item.reject();
        return false;
    }

    // release ownership and post to the dispatcher window
    // the item is move-constructed on the heap to make it "transient"
    PostMessage(_handle, WM_DISPATCHER_SUBMIT_ITEM, 0, reinterpret_cast<LPARAM>(new DispatcherItem{std::forward<DispatcherItem>(item)}));
    return true;
}

UINT MessageThreadDispatcher::WM_DISPATCHER_SUBMIT_ITEM = RegisterWindowMessage(_T("asyncrt.windows.submit_item"));
ATOM MessageThreadDispatcher::WND_CLASS = 0;

LRESULT __stdcall MessageThreadDispatcher::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    assert(hwnd != nullptr);
    if (!hwnd) {
        return -1;
    }

    const auto instance = reinterpret_cast<MessageThreadDispatcher*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (WM_DISPATCHER_SUBMIT_ITEM == msg) {
        assert(lp != 0);
        assert(instance != nullptr);
        if (!(lp && instance)) {
            return -1;
        }

        // take ownership
        const std::unique_ptr<DispatcherItem> item{reinterpret_cast<DispatcherItem*>(lp)};
        if (!instance->is_stopped()) {
            item->invoke();
        } else {
            item->reject();
        }

        return 0;
    }

    // custom messages have been processed
    switch (msg) {
        case WM_CREATE: {
            const auto create_struct = reinterpret_cast<LPCREATESTRUCT>(lp);
            assert(create_struct != nullptr);
            if (!create_struct) {
                return -1;
            }

            assert(create_struct->lpCreateParams != nullptr);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
            return 0;
        }
        case WM_DESTROY: {
            assert(instance != nullptr);
            if (!instance) {
                return -1;
            }
            instance->stop();
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

MessageThreadDispatcher::MessageThreadDispatcher() {
    if (!WND_CLASS) {
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.lpszClassName = DISPATCHER_CLASS_NAME;
        wc.lpfnWndProc = &wnd_proc;

        WND_CLASS = RegisterClassEx(&wc);
    }

    if (!WND_CLASS) {
        // @TODO throw windows exception GetLastError
    }

    // window presence is an invariant
    _handle = CreateWindowEx(
        0,
        DISPATCHER_CLASS_NAME,
        nullptr,
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    if (!_handle) {
        // @TODO throw windows exception GetLastError
    }
}

MessageThreadDispatcher::~MessageThreadDispatcher() {
    DestroyWindow(_handle);
    _handle = nullptr;
}

VOID CALLBACK ThreadPoolDispatcher::thread_proc([[maybe_unused]] PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work) {
    assert(context != nullptr);
    assert(work != nullptr);

    if (!(context && work)) {
        return;
    }

    const WorkPtr scoped_work{work, &CloseThreadpoolWork};
    const std::unique_ptr<ThreadProcData> scoped_context{static_cast<ThreadProcData*>(context)};

    if (scoped_context->dispatcher->is_stopped()) {
        scoped_context->item.reject();
        return;
    }

    scoped_context->item.invoke();
}

ThreadPoolDispatcher::ThreadPoolDispatcher() {
    _pool = PoolPtr{CreateThreadpool(nullptr), &CloseThreadpool};
    if (!_pool) {
        // @TODO throw
        return;
    }

    _env.init(_pool);

    _cleanup_group = CleanupGroupPtr{CreateThreadpoolCleanupGroup(), &CloseThreadpoolCleanupGroup};
    if (!_cleanup_group) {
        DestroyThreadpoolEnvironment(&_env.env);
        return;
    }

    SetThreadpoolCallbackCleanupGroup(&_env.env, _cleanup_group.get(), &cleanup_group_callback);
}

VOID CALLBACK ThreadPoolDispatcher::cleanup_group_callback(PVOID object, [[maybe_unused]] PVOID context) {
    assert(object != nullptr);
    // context not necessary so far
    if (!object) {
        return;
    }

    // reject all the pending items
    const std::unique_ptr<ThreadProcData> proc_data(static_cast<ThreadProcData*>(object));
    proc_data->item.reject();
}

ThreadPoolDispatcher::~ThreadPoolDispatcher() {
    const std::unique_lock lock{_pool_mtx};
    CloseThreadpoolCleanupGroupMembers(_cleanup_group.get(), TRUE, nullptr);
}

bool ThreadPoolDispatcher::submit(DispatcherItem &&item) {
    if (is_stopped()) {
        item.reject();
        return false;
    }

    auto context = std::make_unique<ThreadProcData>(std::forward<DispatcherItem>(item), this);

    const std::unique_lock lock{_pool_mtx}; // now accessing the pool
    const auto work = CreateThreadpoolWork(
        &thread_proc,
        context.get(),
        &_env.env
    );

    assert(work != nullptr);
    if (!work) {
        context->item.reject();
        return false;
    }

    std::ignore = context.release();
    SubmitThreadpoolWork(work);
    return true;
}

auto ThreadPoolDispatcher::instance() -> std::shared_ptr<Dispatcher>{
    static std::shared_ptr<ThreadPoolDispatcher> instance;
    if (!instance) {
        instance = std::make_shared<ThreadPoolDispatcher>();
    }
    return instance;
}

void ThreadPoolDispatcher::ScopedEnvironment::init(const PoolPtr &pool) {
    if (initialized) {
        return;
    }

    InitializeThreadpoolEnvironment(&env);
    SetThreadpoolCallbackPool(&env, pool.get());
}

ThreadPoolDispatcher::ScopedEnvironment::~ScopedEnvironment() {
    if (initialized) {
        DestroyThreadpoolEnvironment(&env);
    }
}

}