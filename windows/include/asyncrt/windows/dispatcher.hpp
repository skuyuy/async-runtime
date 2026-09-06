#pragma once
#include <coroutine>
#include <functional>
#include <memory>
#include <mutex>
#include <deque>

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

#include <asyncrt/core/resume_on.hpp>

namespace asyncrt::windows {

class DispatcherItem {
public:
    DispatcherItem() = default;
    explicit DispatcherItem(std::move_only_function<void()> callable, std::move_only_function<void()> destroy = {});
    explicit DispatcherItem(std::coroutine_handle<> handle);

    // @TODO utility -> DISABLE_COPYABLE(Class)
    DispatcherItem(const DispatcherItem &) = delete;
    DispatcherItem &operator=(const DispatcherItem &) = delete;

    DispatcherItem(DispatcherItem &&other) noexcept;
    DispatcherItem &operator=(DispatcherItem &&other) noexcept;

    void operator()();

    ~DispatcherItem();
private:
    std::move_only_function<void()> _callable; // invocation callback
    std::move_only_function<void()> _destroy; // optional rejection callback
};

// @TODO common windows header with exceptions etc

// base class for dispatchers
class Dispatcher {
public:
    virtual ~Dispatcher() = default;

    void start();
    virtual void stop(bool clear_pending = false) noexcept;
    [[nodiscard]] bool is_stopped() const noexcept;

    static auto current() -> std::shared_ptr<Dispatcher>;
    static void set_current(const std::shared_ptr<Dispatcher> &dispatcher);
    static void shutdown_environment();

    void run(DispatcherItem &&item);
    bool submit(DispatcherItem &&item);
protected:
    virtual bool push(DispatcherItem &&item) = 0;

    void process_pending();
    void enqueue_pending(DispatcherItem &&item);
private:
    std::atomic_bool _stopped{false};

    std::mutex _mtx;
    std::deque<DispatcherItem> _pending_items;
};

// dispatcher created on a win32 thread that pumps messages
class MessageThreadDispatcher final : public Dispatcher {
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
public:
    // submit dispatcher items
    // WPARAM: unused
    // LPARAM: pointer to DispatcherItem
    static UINT WM_DISPATCHER_SUBMIT_ITEM;
    static UINT WM_DISPATCHER_PROCESS_PENDING;
    static ATOM WND_CLASS;

    MessageThreadDispatcher();
    ~MessageThreadDispatcher() override;

    [[nodiscard]]
    bool push(DispatcherItem &&item) override;
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

        TP_CALLBACK_ENVIRON env{};
        bool initialized{false};
    };

    struct ThreadProcData {
        DispatcherItem item;
        ThreadPoolDispatcher* dispatcher{nullptr};

        void operator()() {
            dispatcher->run(std::move(item));
        }
    };
    static VOID CALLBACK thread_proc(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work);
    static VOID CALLBACK cleanup_group_callback(PVOID object, PVOID context);
public:
    ThreadPoolDispatcher();
    ~ThreadPoolDispatcher() override;

    bool push(DispatcherItem &&item) override;
    void stop(bool clear_pending) noexcept override;

    static auto instance() -> std::shared_ptr<Dispatcher>;
private:
    std::mutex _pool_mtx;
    PoolPtr _pool{nullptr, nullptr};
    ScopedEnvironment _env;
    CleanupGroupPtr _cleanup_group{nullptr, nullptr};
};

}