#pragma once
#include <async/context.hpp>

// process work items in the dispatch queue; used as a timer or sent as a message if processing should be manually invoked for some reason
// WPARAM: unused
// LPARAM: unused
#define WM_WIN_CTX_PROCESS  ( WM_USER + 2 )

class WinAppSynchronizationContext : public async::SynchronizationContext
{
    static LRESULT __stdcall WinAppSynchronizationContextProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);
public:
    WinAppSynchronizationContext();
    ~WinAppSynchronizationContext();

    void Send(std::unique_ptr<async::IDispatcherOperation> operation) override;
    void Post(std::unique_ptr<async::IDispatcherOperation> operation) override;
    void Process() override;
    void Start() override;
    void Stop() override;
    void Shutdown() override;
private:

    HWND _handle{nullptr};
    std::atomic_bool _stopped{true};
    async::Dispatcher _dispatcher; // -> in context template, as shared dispatcher type
};
