// DebugPrint_MonitorDlg.h : header file
//

#if !defined(AFX_DEBUGPRINT_MONITORDLG_H__C6D3AA81_56D6_4846_8A75_162CCFE3B0F5__INCLUDED_)
#define AFX_DEBUGPRINT_MONITORDLG_H__C6D3AA81_56D6_4846_8A75_162CCFE3B0F5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CDebugPrint_MonitorDlg dialog

//#include  "NetCA_HARDWARE.h"  
  
#define  UnknownDevice  TEXT("<Unknown  Device>") 


class CDebugPrint_MonitorDlg : public CDialog
{
// Construction
public:
	CDebugPrint_MonitorDlg(CWnd* pParent = NULL);	// standard constructor
public:
	/*
	BOOL IsDisableable(DWORD  SelectedItem,  HDEVINFO  hDevInfo);
	BOOL IsDisabled(DWORD  SelectedItem,  HDEVINFO  hDevInfo);
	BOOL StateChange(DWORD  NewState,  DWORD  SelectedItem,  HDEVINFO  hDevInfo);
    BOOL GetRegistryProperty(HDEVINFO  DeviceInfoSet,  PSP_DEVINFO_DATA  DeviceInfoData,  ULONG  Property,  PVOID  Buffer,  PULONG  Length);
    BOOL ConstructDeviceName(HDEVINFO  DeviceInfoSet,  PSP_DEVINFO_DATA  DeviceInfoData,  PVOID  Buffer,  PULONG  Length);
	BOOL EnumAddDevices(BOOL  ShowHidden,  HDEVINFO  hDevInfo);
	STDMETHODIMP  Disable(BSTR  DriverID,  long  *pVal);
	*/
	// Dialog Data
	//{{AFX_DATA(CDebugPrint_MonitorDlg)
	enum { IDD = IDD_DEBUGPRINT_MONITOR_DIALOG };
	CListCtrl	m_EventListCtr_Record;
	CListCtrl	m_EventListCtr_Process;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDebugPrint_MonitorDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	int m_RecordNum,m_ProcessNum;

	CFont m_fnt;//×ÖÌå

	// Generated message map functions
	//{{AFX_MSG(CDebugPrint_MonitorDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnButtonProcess();
	afx_msg void OnButtonRecord();
	afx_msg void OnButtonInstallDbg();
	afx_msg void OnButtonUninstallDbg();
	afx_msg void OnButtonInstallTdi();
	afx_msg void OnButtonUninstallTdi();
	//}}AFX_MSG
	afx_msg LRESULT OnSendMessage(WPARAM wParam,LPARAM lParam);
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DEBUGPRINT_MONITORDLG_H__C6D3AA81_56D6_4846_8A75_162CCFE3B0F5__INCLUDED_)
