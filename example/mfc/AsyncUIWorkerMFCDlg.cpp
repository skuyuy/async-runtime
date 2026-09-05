
// AsyncUIWorkerMFCDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "AsyncUIWorkerMFCDlg.h"
#include "afxdialogex.h"
#include "resource.h"

#include <chrono>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CAsyncUIWorkerMFCDlg dialog



CAsyncUIWorkerMFCDlg::CAsyncUIWorkerMFCDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_ASYNCUIWORKERMFC_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CAsyncUIWorkerMFCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAsyncUIWorkerMFCDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, &CAsyncUIWorkerMFCDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CAsyncUIWorkerMFCDlg::OnBnClickedButton2)
END_MESSAGE_MAP()


// CAsyncUIWorkerMFCDlg message handlers

BOOL CAsyncUIWorkerMFCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CAsyncUIWorkerMFCDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CAsyncUIWorkerMFCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CAsyncUIWorkerMFCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

std::wstring LongWork(std::stop_token st)
{
	std::mutex mtx;
	std::condition_variable_any cv;
	std::stop_callback stopCallback{ st, [&cv] { cv.notify_one(); } };

	std::unique_lock l{ mtx };
	auto start = std::chrono::system_clock::now();
	cv.wait_for(l, std::chrono::seconds{ 5 }, [st] { return st.stop_requested(); });

	if (st.stop_requested())
	{
		return std::format(L"Task1 was cancelled after {}s", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count());
	}
	return std::format(L"Task1 finished after {}s", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count());
}

//Task<void> OnBnClickedButton1Async(std::stop_token st)
//{
//	// Cooperative cancellation (the task handles cancellation by itself)
//	// -> Cancel() should not be invoked; instead the cancellation is reflected by the awaited value
//	GetDlgItem(IDC_BUTTON2)->SetWindowTextW(L"Cancel cooperatively");
//	auto task1 = RunOnWorker(LongWork, st); // use the provided stop token; this way we dont need to manually cancel the task
//	auto task2 = RunOnWorker([] {
//		Sleep(10000); // intentionally using something long
//	});
//	
//	auto text = co_await task1;
//	GetDlgItem(IDC_STATIC)->SetWindowTextW(text.c_str());
//	GetDlgItem(IDC_BUTTON2)->SetWindowTextW(L"Cancel non-cooperatively");
//
//	// !! second task will still be executed here, so we also need to return
//	if (st.stop_requested())
//	{
//		co_return;
//	}
//
//	try
//	{
//		co_await task2;
//		GetDlgItem(IDC_STATIC)->SetWindowText(L"Task2 completed");
//	}
//	catch (const Cancelled&)
//	{
//		GetDlgItem(IDC_STATIC)->SetWindowText(L"Task2 has been cancelled but did not handle the cancellation (Timeout)");
//	}
//}

asyncrt::windows::Task<std::wstring> LongWorkAsync(std::stop_token st) {
    // co_await asyncrt::windows::resume_on_threadpool();
    co_return LongWork(st);
}

asyncrt::windows::Task<void> LongWork2Async() {
    // co_await asyncrt::windows::resume_on_threadpool();
    Sleep(10000);
    co_return;
}

//IMPLEMENT_ASYNC_HANDLER(CAsyncUIWorkerMFCDlg, OnBnClickedButton1, WPARAM wp, LPARAM lp)
void CAsyncUIWorkerMFCDlg::OnBnClickedButton1() {
    OnBnClickedButton1_Async();
}

asyncrt::windows::Task<void> CAsyncUIWorkerMFCDlg::OnBnClickedButton1_Async() {
    /*
     * TODO put everything together in a RuntimeTraits<> trait struct so we can just do: asyncrt::current_context<WindowsRuntime>()
     */
        const auto ui_context = asyncrt::windows::Dispatcher::current();

	// Cooperative cancellation (the task handles cancellation by itself)
	// -> Cancel() should not be invoked; instead the cancellation is reflected by the awaited value
	GetDlgItem(IDC_BUTTON2)->SetWindowText(_T("Cancel cooperatively"));

        co_await asyncrt::windows::resume_on_threadpool();
	auto text = co_await LongWorkAsync(_stopSource.get_token());

        co_await asyncrt::core::resume_on(ui_context);

        GetDlgItem(IDC_STATIC)->SetWindowText(text.c_str());
	GetDlgItem(IDC_BUTTON2)->SetWindowText(_T("Cancel non-cooperatively"));

	// !! second task will still be executed here, so we also need to return
	if (_stopSource.stop_requested())
	{
	    co_return;
	}

        co_await asyncrt::windows::resume_on_threadpool();
	co_await LongWork2Async();

        co_await asyncrt::core::resume_on(ui_context);
        GetDlgItem(IDC_STATIC)->SetWindowText(_T("Task2 completed"));
}

void CAsyncUIWorkerMFCDlg::OnBnClickedButton2()
{
    _stopSource.request_stop();
}
