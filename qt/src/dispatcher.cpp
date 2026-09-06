#include <asyncrt/qt/dispatcher.hpp>

#include <QThreadPool>
#include <QCoreApplication>
#include <utility>

namespace asyncrt::qt {

QEvent::Type DispatcherEvent::EVENT_TYPE{static_cast<Type>(registerEventType())};

CoroutineDispatcherEvent::CoroutineDispatcherEvent(const std::coroutine_handle<> handle) {
    _handle = handle;
}

CoroutineDispatcherEvent::~CoroutineDispatcherEvent() {
    // clean up handles that have not been released
    if (_handle) {
        _handle.destroy();
    }
}

void CoroutineDispatcherEvent::operator()() {
    if (_handle) {
        std::exchange(_handle, {}).resume(); // release the handle and resume
    }
}

QEvent * CoroutineDispatcherEvent::clone() const {
    Q_ASSERT_X(false, __FUNCTION__, "Cloning a CoroutineDispatcherEvent is not allowed");
    return nullptr;
}

DelegateDispatcherEvent::DelegateDispatcherEvent(std::move_only_function<void()> delegate)
    : _delegate(std::move(delegate)) {
}

void DelegateDispatcherEvent::operator()() {
    if (_delegate) {
        _delegate();
    }
}

EventLoopDispatcher::EventLoopDispatcher(QObject *parent)
    : QObject(parent)
    , Dispatcher() {
}

EventLoopDispatcher * EventLoopDispatcher::application_dispatcher() {
    // automatic cleanup via Qt object tree
    static EventLoopDispatcher *instance{nullptr};
    if (!instance) {
        // lazily create an event loop for the application
        // QCoreApplication owns this dispatcher and is responsible for cleaning it up
        instance = new EventLoopDispatcher{QCoreApplication::instance()};
    }
    return instance;
}

bool EventLoopDispatcher::submit(DispatcherEvent *event) {
    if (!event) {
        return false;
    }
    QCoreApplication::postEvent(this, event);
    return true;
}

bool EventLoopDispatcher::post_coroutine(std::coroutine_handle<> handle) {
    return submit<CoroutineDispatcherEvent>(handle);
}

bool EventLoopDispatcher::post_delegate(std::move_only_function<void()> delegate) {
    return submit<DelegateDispatcherEvent>(std::forward<decltype(delegate)>(delegate));
}

bool EventLoopDispatcher::event(QEvent *event) {
    if (QEvent::Quit == event->type()) {
        stop();
        return QObject::event(event);
    }

    if (DispatcherEvent::EVENT_TYPE != event->type()) {
        event->ignore();
        return QObject::event(event);
    }

    /*
     * If the dispatcher is stopped inbetween an event submission and the event processing
     * we re-post the event back on the pending queue
     * The dispatcher will drain the pending queue first every time event is called
     */
    if (!_stopped) {
        // @TODO pending queue; make it its own type: DispatcherQueue
        // otherwise events will be swallowed if the dispatcher is stopped
        (*reinterpret_cast<DispatcherEvent*>(event))();
        event->accept();
    }
    return true;
}

void EventLoopDispatcher::stop() {
    _stopped = true;
}

void ThreadPoolDispatcher::safe_delete_threadpool(const QThreadPool *instance) {
    if (instance != QThreadPool::globalInstance()) {
        delete instance;
    }
}

ThreadPoolDispatcher::ThreadPoolDispatcher(std::shared_ptr<QThreadPool> instance)
    : Dispatcher()
    , _instance{std::move(instance)} {
    // explicitly /dont/ transfer ownership since it could just be a reference to an existing thread pool (maybe the application maintains multiple ones already)
    // if ownership should be transferred, use ThreadPoolDispatcher::take(QThreadPool*) to transfer ownership
}

bool ThreadPoolDispatcher::post_coroutine(const std::coroutine_handle<> handle) {
    if (!_instance) {
        return false;
    }

    /*
     * QThreadPool offers no cancellation support, so it is hacked together via std::stop_token + a custom QRunnable
     * when QThreadPool is destroyed, it waits for all runnable instances to complete.
     *
     * When a new coroutine handle is submitted from an in-flight task (e.g. resume_on) it will notice that the
     * threadpool is "closed" and not resume the coroutine
     *
     * Once the runnable is finished and still owns the handle, it will be destroyed by the threadpool and handle cleanup in its destructor
     */
    class CoroutineRunnable : public QRunnable {
    public:
        explicit CoroutineRunnable(const std::coroutine_handle<> handle, std::stop_token st)
            : _handle{handle}
            , _st{std::move(st)}
        {}

        ~CoroutineRunnable() override {
            // clean up handles that did not get invoked
            if (_handle) {
                _handle.destroy();
            }
        }

        void run() override {
            if (_st.stop_requested()) {
                // abort running
                // the handle will be cleaned up by the destructor
                return;
            }

            if (_handle) {
                // release the handle and run it
                std::exchange(_handle, {}).resume();
            }
        }
    private:
        std::stop_token _st;
        std::coroutine_handle<> _handle;
    };

    _instance->start(new CoroutineRunnable{handle, _stop_src.get_token()});
    return true;
}

bool ThreadPoolDispatcher::post_delegate(std::move_only_function<void()> delegate) {
    if (!_instance) {
        return false;
    }
    _instance->start(std::move(delegate));
    return true;
}

void ThreadPoolDispatcher::stop() {
    _stop_src.request_stop();

    if (_instance) {
        _instance->waitForDone();
    }
}

}