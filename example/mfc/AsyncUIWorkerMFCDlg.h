
// AsyncUIWorkerMFCDlg.h : header file
//

#pragma once
#include "AsyncUIWorker.h"
#include <async/task.hpp>


// CAsyncUIWorkerMFCDlg dialog
class CAsyncUIWorkerMFCDlg : public CDialogEx
{
// Construction
public:
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
