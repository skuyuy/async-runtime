#include "pch.h"
#include "SynchronizationContext.h"

#include <Windows.h>

#include <atomic>

int OnMsgWinContextProcess(HWND hWnd)
{
    ASSERT(hWnd != nullptr);
    if (!hWnd)
    {
        return -1;
    }
    const auto synchronizationContextPtr = reinterpret_cast<WinAppSynchronizationContext*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    ASSERT(synchronizationContextPtr != nullptr);
    if (!synchronizationContextPtr)
    {
        return -1;
    }
    
    synchronizationContextPtr->Process();
    return 0;
}

LRESULT __stdcall WinAppSynchronizationContext::WinAppSynchronizationContextProc(HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    switch (nMsg)
    {
    case WM_CREATE: {
        const auto createStructPtr = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        ASSERT(createStructPtr);
        if (!createStructPtr)
        {
            return -1;
        }

        auto ctx = static_cast<WinAppSynchronizationContext*>(createStructPtr->lpCreateParams);
        ctx->_handle = hWnd;
        ctx->Start();

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
        return 0;
    }
    case WM_WIN_CTX_PROCESS:
        return OnMsgWinContextProcess(hWnd);
    case WM_TIMER: {
        if (WM_WIN_CTX_PROCESS == wParam)
        {
            return OnMsgWinContextProcess(hWnd);
        }
        ASSERT(false);
        return -1; // unknown timer ID
    }
    default:
        return DefWindowProcW(hWnd, nMsg, wParam, lParam);
    }
}

#define WIN_APP_SYNCHRONIZATION_CONTEXT_MESSAGE_WND_CLASSNAME L"WinAppSynchronizationContextMessageWnd"

WinAppSynchronizationContext::WinAppSynchronizationContext()
{
    static ATOM wndClass{0};
    if(!wndClass) {
        WNDCLASSEXW wc{ };
        wc.cbSize = sizeof(wc);
        wc.hInstance = AfxGetInstanceHandle();
        wc.lpszClassName = WIN_APP_SYNCHRONIZATION_CONTEXT_MESSAGE_WND_CLASSNAME;
        wc.lpfnWndProc = &WinAppSynchronizationContextProc;

        wndClass = RegisterClassExW(&wc);
    }

    if (!wndClass)
    {
        throw std::invalid_argument("Could not register message window");
    }

    _handle = CreateWindowExW(
        0,
        WIN_APP_SYNCHRONIZATION_CONTEXT_MESSAGE_WND_CLASSNAME,
        nullptr,
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        AfxGetInstanceHandle(),
        this
    );
    if (!_handle)
    {
        throw std::invalid_argument("Could not create message window");
    }
}

WinAppSynchronizationContext::~WinAppSynchronizationContext() {
    WinAppSynchronizationContext::Shutdown();
}

void WinAppSynchronizationContext::Send(const std::unique_ptr<async::IDispatcherOperation> operation) {
    // no handle means we fully shut down, in which case we do not allow sending
    // mainly has to do with semantics and stating that "shut down" meeans completely shut down
    if (!_handle) {
        return;
    }
    // synchronously invoke the item
    if (operation)
    {
        operation->Invoke();
    }
}

void WinAppSynchronizationContext::Post(std::unique_ptr<async::IDispatcherOperation> operation)
{
    // no handle means we fully shut down, in which case we do not allow posting
    if (!_handle) {
        return;
    }
    // push the item on the dispatcher queue without waiting for it
    // we allow this even if the context is stopped, it will resume processing later sometime
    _dispatcher.Push(std::move(operation));
}

void WinAppSynchronizationContext::Process()
{
    // called by WM_TIMER
    if (!_stopped)
    {
        _dispatcher.ProcessNextOperations();
    }
}

void WinAppSynchronizationContext::Start() {
    if (!_stopped) {
        // already running, no-op
        return;
    }
    _stopped = false;
    SetTimer(_handle, WM_WIN_CTX_PROCESS, 50, nullptr);
}

void WinAppSynchronizationContext::Stop()
{
    if (_stopped) {
        return;
    }

    // DONT clear the dispatcher, we might want to re-start processing later
    _stopped = true;
    KillTimer(_handle, WM_WIN_CTX_PROCESS);
}

void WinAppSynchronizationContext::Shutdown() {
    Stop();

    _dispatcher.Clear(); // clear the dispatcher as well
    DestroyWindow(_handle);
    _handle = nullptr;
}
