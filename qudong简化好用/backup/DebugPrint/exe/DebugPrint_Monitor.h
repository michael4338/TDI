// DebugPrint_Monitor.h : main header file for the DEBUGPRINT_MONITOR application
//

#if !defined(AFX_DEBUGPRINT_MONITOR_H__B859E186_A43F_4046_95D6_EDC80AEE7760__INCLUDED_)
#define AFX_DEBUGPRINT_MONITOR_H__B859E186_A43F_4046_95D6_EDC80AEE7760__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CDebugPrint_MonitorApp:
// See DebugPrint_Monitor.cpp for the implementation of this class
//

#define Version "1.03"

/////////////////////////////////////////////////////////////////////////////
//	Listener thread globals

void StartListener();
void StopListener();

class CDebugPrint_MonitorApp : public CWinApp
{
public:
	CDebugPrint_MonitorApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDebugPrint_MonitorApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CDebugPrint_MonitorApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DEBUGPRINT_MONITOR_H__B859E186_A43F_4046_95D6_EDC80AEE7760__INCLUDED_)
