#include <cassert>
#include <stdexcept>
#include <future>
#include <format>

#include <asyncrt/windows/dispatcher.hpp>
#include <tchar.h>

#define DISPATCHER_CLASS_NAME _T("asyncrt.windows.message_thread_dispatcher")

namespace asyncrt::windows {

namespace tls {

// any thread-local dispatcher, open to custom implementations
thread_local std::shared_ptr<Dispatcher> local_dispatcher;

}

std::shared_ptr<Dispatcher> Dispatcher::current() {
    // using a global thread-pool as a fallback, if no local dispatcher is configured
    if(!tls::local_dispatcher) {
        return ThreadPoolDispatcher::instance();
    }
    return tls::local_dispatcher;
}

void Dispatcher::set_current(const std::shared_ptr<Dispatcher> &dispatcher){
    tls::local_dispatcher = dispatcher;
}

void Dispatcher::shutdown_environment(){
    // stop all dispatchers and delete the thread local one
    // the threadpool dispatcher will be cleaned up at static shutdown and should persist
    if (tls::local_dispatcher) {
        tls::local_dispatcher->stop(true);
        tls::local_dispatcher = nullptr;
    }
    if (const auto threadpool_ctx = ThreadPoolDispatcher::instance()) {
        threadpool_ctx->stop(true);
    }
}

void Dispatcher::run(DispatcherItem &&item) {
    if (!is_stopped()) { // it could be that inbetween submit and run, the dispatcher was stopped
        item();
        return;
    }

    enqueue_pending(std::forward<DispatcherItem>(item));
}

void Dispatcher::process_pending() {
    std::unique_lock lock{_mtx};
    while (!_pending_items.empty()) {
        auto item = std::move(_pending_items.front());
        _pending_items.pop_front();
        item();
    }
}

void Dispatcher::enqueue_pending(DispatcherItem &&item){
    std::unique_lock lock{_mtx};
    _pending_items.push_back(std::forward<DispatcherItem>(item));
}

DispatcherItem::DispatcherItem(std::move_only_function<void()> callable, std::move_only_function<void()> destroy)
    : _callable(std::move(callable))
    , _destroy(std::move(destroy)) {
}

DispatcherItem::DispatcherItem(std::coroutine_handle<> handle) {
    assert(handle);
    if (!handle) {
        _callable = []{};
        _destroy = []{};
        return;
    }

    // if the dispatcher rejects the handle, it must be cleaned up
    // example scenario: stopping the runtime while some tasks are still in-flight
    // if the runtime is stopped, it rejects the submitted items
    // in order for the handles to not leak, we need to destroy them if they are rejected
    // therefore we need to share the object

    auto shared_handle = std::make_shared<std::coroutine_handle<>>(handle);
    _callable = [shared_handle] {
        shared_handle->resume();
        *shared_handle = {}; // replace the underlying object with an empty handle since it is now detached
    };

    _destroy = [shared_handle] {
        if (*shared_handle) {
            // only destroy the handle if it is still valid (-> not resumed and reset by _callable)
            shared_handle->destroy();
        }
    };
}

DispatcherItem & DispatcherItem::operator=(DispatcherItem &&other) noexcept {
    // invalidate the other function wrappers and replace them with noops
    _callable = std::exchange(other._callable, {});
    _destroy = std::exchange(other._destroy, {});
    return *this;
}

void DispatcherItem::operator()() {
    if (_callable) {
        _callable();
    }
}

DispatcherItem::DispatcherItem(DispatcherItem &&other) noexcept
    : _callable(std::move(other._callable))
    , _destroy(std::move(other._destroy)) {
}

DispatcherItem::~DispatcherItem() {
    if (_destroy) {
        _destroy();
    }
}

void Dispatcher::start() {
    // process pending /just to be sure/
    process_pending();
    // if we were stopped, flip
    if (_stopped) {
        _stopped = false;
    }
}

bool Dispatcher::submit(DispatcherItem &&item){
    if (!is_stopped()) {
        return push(std::forward<DispatcherItem>(item));
    }

    std::unique_lock lock{_mtx};
    _pending_items.push_back(std::move(item));
    return false;
}


void Dispatcher::stop(bool clear_pending) noexcept {
    _stopped = true;

    // give other threads the chance to complete pending push before we lock
    std::unique_lock lock{_mtx};
    _pending_items.clear();
}

bool Dispatcher::is_stopped() const noexcept {
    return _stopped;
}

bool MessageThreadDispatcher::push(DispatcherItem &&item) {
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
        instance->run(std::move(*(item))); // move so item is reset
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

#ifdef _DEBUG
    std::ignore = SetThreadDescription(GetCurrentThread(), std::format(L"asyncrt.windows.ThreadPoolDispatcher/{:x}", reinterpret_cast<std::uintptr_t>(instance)).c_str());
#endif

    const WorkPtr scoped_work{work, &CloseThreadpoolWork};
    const std::unique_ptr<ThreadProcData> scoped_context{static_cast<ThreadProcData*>(context)};
    (*scoped_context)();
}

ThreadPoolDispatcher::ThreadPoolDispatcher() {
    _pool = PoolPtr{CreateThreadpool(nullptr), &CloseThreadpool};
    if (!_pool) {
        // @TODO throw
        return;
    }
    SetThreadpoolThreadMinimum(_pool.get(), 1); // at least one
    SetThreadpoolThreadMaximum(_pool.get(), 16); // should be ok, we dont expect the system to get hammered by that many long running invokes

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

    // delete the context item
    delete static_cast<ThreadProcData*>(object);
}

ThreadPoolDispatcher::~ThreadPoolDispatcher() {
    const std::unique_lock lock{_pool_mtx};
    CloseThreadpoolCleanupGroupMembers(_cleanup_group.get(), TRUE, nullptr);
}

bool ThreadPoolDispatcher::push(DispatcherItem &&item) {
    auto context = std::make_unique<ThreadProcData>(std::forward<DispatcherItem>(item), this);

    const std::unique_lock lock{_pool_mtx}; // now accessing the pool
    const auto work = CreateThreadpoolWork(
        &thread_proc,
        context.get(),
        &_env.env
    );

    assert(work != nullptr);
    if (!work) {
        // failed to create the work so this will be moved to pending
        // this case is very unlikely
        enqueue_pending(std::forward<DispatcherItem>(item));
        return false;
    }

    std::ignore = context.release();
    SubmitThreadpoolWork(work);
    return true;
}

void ThreadPoolDispatcher::stop(bool clear_pending) noexcept {
    Dispatcher::stop(clear_pending);
    CloseThreadpoolCleanupGroupMembers(_cleanup_group.get(), TRUE, nullptr);
}

auto ThreadPoolDispatcher::instance() -> std::shared_ptr<Dispatcher> {
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