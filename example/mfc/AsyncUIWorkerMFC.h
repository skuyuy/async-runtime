
// AsyncUIWorkerMFC.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

// CAsyncUIWorkerMFCApp:
// See AsyncUIWorkerMFC.cpp for the implementation of this class
//

class CAsyncUIWorkerMFCApp : public CWinApp
{
public:
	CAsyncUIWorkerMFCApp();

// Overrides
public:
	 BOOL InitInstance()override;
	int ExitInstance() override;


// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CAsyncUIWorkerMFCApp theApp;
