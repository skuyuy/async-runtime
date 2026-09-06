
// AsyncUIWorkerMFCDlg.h : header file
//

#pragma once
#include "AsyncUIWorker.h"
#include <asyncrt/windows/task.hpp>


// CAsyncUIWorkerMFCDlg dialog
class CAsyncUIWorkerMFCDlg : public CDialogEx
{
// Construction
public:
    // @TODO
    // class CAsyncUIWorkerMFCDlg : public CDialogEx, asyncrt::Lifetime<CAsyncUIWorkerMFCDlg>
    // Task<void> OnButton1BnClicked(asyncrt::Lifetime<CAsyncUIWorkerMFCDlg> dlg) {
    //     auto dialog = dlg.weak(); // weak ptr to lifetime token
    //     // async work...
    //     if(weak.expired()) co_return;
    // }
    // or as member:
    // Task<void> CAsyncUIWorkerMFCDlg::OnButton1BnClicked() {
    //     auto dialog = weak();
    //     if(weak.expired()) co_return;
    // }

    CAsyncUIWorkerMFCDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ASYNCUIWORKERMFC_DIALOG };
#endif

	protected:
	void DoDataExchange(CDataExchange* pDX) override;	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	BOOL OnInitDialog() override;
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	DECLARE_ASYNC_HANDLER(OnBnClickedButton1, WPARAM wp, LPARAM lp)

	afx_msg void OnBnClickedButton2();
private:
	std::stop_source _stopSource;
};
