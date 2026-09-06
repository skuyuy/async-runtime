#pragma once
#include <coroutine>
#include <functional>
#include <source_location>

#include <QEvent>
#include <QThreadPool>
#include <QDebug>

namespace asyncrt::qt {

class DispatcherEvent : public QEvent {
public:
    static Type EVENT_TYPE;

    DispatcherEvent()
        : QEvent(EVENT_TYPE)
    {}

    virtual void operator()() = 0;

    std::source_location _loc;
};

class CoroutineDispatcherEvent : public DispatcherEvent {
public:
    explicit CoroutineDispatcherEvent(std::coroutine_handle<> handle);
    ~CoroutineDispatcherEvent() override;

    void operator()() override;

    [[nodiscard]] QEvent *clone() const override;
private:
    std::coroutine_handle<> _handle;
};

class DelegateDispatcherEvent : public DispatcherEvent {
public:
    explicit DelegateDispatcherEvent(std::move_only_function<void()> delegate);
    void operator()() override;
private:
    std::move_only_function<void()> _delegate;
};

class Dispatcher {
public:
    // can be inserted anywhere in the object tree, which means that any object could have its own dispatcher for isolated stuff; or use the global application dispatcher
    // example: a widget with progress could have its own dispatcher to run isolated coroutines
    virtual ~Dispatcher() = default;

    virtual bool post_coroutine(std::coroutine_handle<> handle) = 0;
    // @TODO support stop_token
    virtual bool post_delegate(std::move_only_function<void()> delegate) = 0;
    virtual void stop() = 0;
private:
    // @TODO DispatchQueue _pending;
};

class EventLoopDispatcher : public QObject, public Dispatcher {
    Q_OBJECT

    bool submit(DispatcherEvent *event);

    template<std::derived_from<DispatcherEvent> EventType, class... Args>
        requires std::constructible_from<EventType, Args...>
    bool submit(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<EventType, Args...>);
public:
    explicit EventLoopDispatcher(QObject *parent = nullptr);
    static EventLoopDispatcher *application_dispatcher(); // dispatcher registered to the QCoreApplication

    bool post_coroutine(std::coroutine_handle<> handle) override;
    bool post_delegate(std::move_only_function<void()> delegate) override;
    void stop() override;

    bool event(QEvent *event) override;
private:
    std::atomic_bool _stopped{false};
};

class ThreadPoolDispatcher : public Dispatcher {
    static void safe_delete_threadpool(const QThreadPool *threadpool);
public:
    explicit ThreadPoolDispatcher(std::shared_ptr<QThreadPool> instance = std::shared_ptr<QThreadPool>{QThreadPool::globalInstance(), &safe_delete_threadpool});

    [[nodiscard]] bool post_coroutine(std::coroutine_handle<> handle) override;
    [[nodiscard]] bool post_delegate(std::move_only_function<void()> delegate) override;
    void stop() override;
private:
    std::stop_source _stop_src;
    std::shared_ptr<QThreadPool> _instance;
};

template <std::derived_from<DispatcherEvent> EventType, class ... Args>
    requires std::constructible_from<EventType, Args...>
bool EventLoopDispatcher::submit(Args &&...args) noexcept(std::is_nothrow_constructible_v<EventType, Args...>) {
    if constexpr (std::is_nothrow_constructible_v<EventType, Args...>) {
        return submit(new EventType{std::forward<Args>(args)...});
    } else {
        try {
            return submit(new EventType{std::forward<Args>(args)...});
        } catch (const std::exception &e) {
            const auto message = QString{"Could not submit event to EventLoopDispatcher [Type=%1]: %2"}.arg(
                typeid(EventType).name(),
                e.what()
            );
            qWarning() << message;
            return false;
        }
    }
}

}