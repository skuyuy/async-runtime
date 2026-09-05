#pragma once
#include <coroutine>
#include <functional>
#include <memory>
#include <mutex>

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

#include <asyncrt/core/resume_on.hpp>

namespace asyncrt::windows {

struct DispatcherItem {
    std::move_only_function<void()> on_invoke; // invocation callback
    std::move_only_function<void()> on_reject; // optional rejection callback

    void invoke(); // cant make operator() because it will fuck with move semantics and constructors
    void reject();
};

auto make_noop_dispatcher_item() -> DispatcherItem;
auto make_dispatcher_item(std::coroutine_handle<> handle) -> DispatcherItem;

// @TODO common windows header with exceptions etc

// base class for dispatchers
class Dispatcher {
public:
    virtual ~Dispatcher() = default;

    void stop() noexcept;
    bool is_stopped() const noexcept;

    // asyncrt::core::Context traits

    bool send(core::PostCoroutineTag, std::coroutine_handle<> handle);
    bool send(std::move_only_function<void()> callable);

    bool post(core::PostCoroutineTag, std::coroutine_handle<> handle);
    bool post(std::move_only_function<void()> callable);

    static auto current() -> std::shared_ptr<Dispatcher>;
    static void set_current(const std::shared_ptr<Dispatcher> &dispatcher);

    virtual bool submit(DispatcherItem &&item) = 0;
private:
    std::atomic_bool _stopped{false};
};

// dispatcher created on a win32 thread that pumps messages
class MessageThreadDispatcher final : public Dispatcher {
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
public:
    // submit dispatcher items
    // WPARAM: unused
    // LPARAM: pointer to DispatcherItem
    static UINT WM_DISPATCHER_SUBMIT_ITEM;
    static ATOM WND_CLASS;

    MessageThreadDispatcher();
    ~MessageThreadDispatcher() override;

    [[nodiscard]]
    bool submit(DispatcherItem &&item) override;
private:
    HWND _handle{nullptr}; // handle to message window
};

// threadpool dispatcher used as a fallback if no dispatcher is configured
// this thread pool needs to be thread-safe as it is accessed from all kinds of threads and the windows threadpool API is /probably/ not threadsafe
class ThreadPoolDispatcher final : public Dispatcher {
    using CallbackEnvPtr = std::unique_ptr<TP_CALLBACK_ENVIRON, decltype(&DestroyThreadpoolEnvironment)>;
    using PoolPtr = std::unique_ptr<TP_POOL, decltype(&CloseThreadpool)>;
    using CleanupGroupPtr = std::unique_ptr<TP_CLEANUP_GROUP, decltype(&CloseThreadpoolCleanupGroup)>;
    using WorkPtr = std::unique_ptr<TP_WORK, decltype(&CloseThreadpoolWork)>;

    struct ScopedEnvironment {
        void init(const PoolPtr &pool);
        ~ScopedEnvironment();

        TP_CALLBACK_ENVIRON env;
        bool initialized{false};
    };

    struct ThreadProcData {
        DispatcherItem item;
        ThreadPoolDispatcher* dispatcher{nullptr};
    };
    static VOID CALLBACK thread_proc(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work);
    static VOID CALLBACK cleanup_group_callback(PVOID object, PVOID context);
public:
    ThreadPoolDispatcher();
    ~ThreadPoolDispatcher() override;

    bool submit(DispatcherItem &&item) override;

    static auto instance() -> std::shared_ptr<Dispatcher>;
private:
    std::mutex _pool_mtx;
    PoolPtr _pool{nullptr, nullptr};
    ScopedEnvironment _env;
    CleanupGroupPtr _cleanup_group{nullptr, nullptr};
};

inline auto resume_on_threadpool() {
    return core::resume_on(ThreadPoolDispatcher::instance());
}

}