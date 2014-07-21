// DebugPrint_MonitorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "DebugPrint_Monitor.h"
#include "DebugPrint_MonitorDlg.h"
#include "listener.h"

#include "string.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
//	Declare static member of DebugPrint_Event class

/////////////////////////////////////////////////////////////////////////////
//	Information passed to ListenThreadFunction

/////////////////////////////////////////////////////////////////////////////
// CDebugPrint_MonitorDlg dialog

CDebugPrint_MonitorDlg::CDebugPrint_MonitorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDebugPrint_MonitorDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDebugPrint_MonitorDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDebugPrint_MonitorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDebugPrint_MonitorDlg)
	DDX_Control(pDX, IDC_LIST_RECORD, m_EventListCtr_Record);
	DDX_Control(pDX, IDC_LIST, m_EventListCtr_Process);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CDebugPrint_MonitorDlg, CDialog)
	//{{AFX_MSG_MAP(CDebugPrint_MonitorDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_PROCESS, OnButtonProcess)
	ON_BN_CLICKED(IDC_BUTTON_RECORD, OnButtonRecord)
	ON_BN_CLICKED(IDC_BUTTON_INSTALL_DBG, OnButtonInstallDbg)
	ON_BN_CLICKED(IDC_BUTTON_UNINSTALL_DBG, OnButtonUninstallDbg)
	ON_BN_CLICKED(IDC_BUTTON_INSTALL_TDI, OnButtonInstallTdi)
	ON_BN_CLICKED(IDC_BUTTON_UNINSTALL_TDI, OnButtonUninstallTdi)
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_SEND_MESSAGE,OnSendMessage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDebugPrint_MonitorDlg message handlers

BOOL CDebugPrint_MonitorDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
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
	m_fnt.CreatePointFont(100,"宋体");
	SetFont(&m_fnt);
	m_EventListCtr_Record.SetFont(&m_fnt);
	m_EventListCtr_Process.SetFont(&m_fnt);

	DWORD dwStyle=GetWindowLong(m_EventListCtr_Process.GetSafeHwnd(),GWL_STYLE);
	dwStyle&=~LVS_TYPEMASK;
	dwStyle|=LVS_REPORT;
	SetWindowLong(m_EventListCtr_Process.GetSafeHwnd(),GWL_STYLE,dwStyle);
    
	DWORD dwStyle2=GetWindowLong(m_EventListCtr_Record.GetSafeHwnd(),GWL_STYLE);
	dwStyle2&=~LVS_TYPEMASK;
	dwStyle2|=LVS_REPORT;
	SetWindowLong(m_EventListCtr_Record.GetSafeHwnd(),GWL_STYLE,dwStyle2);

	//得到视图创建空间大小
	CRect DialogRect;
	CRect ViewRect;
	CRect ButtonRect;
	GetClientRect(&DialogRect);
	GetDlgItem(IDC_BUTTON_PROCESS)->GetWindowRect(&ButtonRect);
	ScreenToClient(&ButtonRect);
	ViewRect.top=ButtonRect.bottom+5;
	ViewRect.bottom=DialogRect.bottom-10;
	ViewRect.left=10;
	ViewRect.right=DialogRect.right-10;
	m_EventListCtr_Process.MoveWindow(ViewRect,FALSE);
	m_EventListCtr_Record.MoveWindow(ViewRect,FALSE);

	//初始化视图列表
    m_EventListCtr_Process.InsertColumn(0,"进程号",LVCFMT_CENTER,100);
	m_EventListCtr_Process.InsertColumn(1,"进程名称",LVCFMT_CENTER,120);
	m_EventListCtr_Process.InsertColumn(2,"时间",LVCFMT_CENTER,100);
	m_EventListCtr_Process.InsertColumn(3,"IP地址",LVCFMT_CENTER,180);
	m_EventListCtr_Process.InsertColumn(4,"操作",LVCFMT_CENTER,100);
	m_EventListCtr_Process.InsertColumn(5,"端口号",LVCFMT_CENTER,100);
	m_EventListCtr_Process.SetExtendedStyle(LVS_EX_GRIDLINES);
    ::SendMessage(m_EventListCtr_Process.m_hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE,
      LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	m_EventListCtr_Record.InsertColumn(0,"进程号",LVCFMT_CENTER,100);
	m_EventListCtr_Record.InsertColumn(1,"进程名称",LVCFMT_CENTER,120);
	m_EventListCtr_Record.InsertColumn(2,"时间",LVCFMT_CENTER,100);
	m_EventListCtr_Record.InsertColumn(3,"IP地址",LVCFMT_CENTER,180);
	m_EventListCtr_Record.InsertColumn(4,"操作",LVCFMT_CENTER,100);
	m_EventListCtr_Record.InsertColumn(5,"端口号",LVCFMT_CENTER,100); 
	m_EventListCtr_Record.SetExtendedStyle(LVS_EX_GRIDLINES);
    ::SendMessage(m_EventListCtr_Record.m_hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE,
      LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    m_EventListCtr_Record.ShowWindow(SW_HIDE);
	EventList::DialogHwnd = this->GetSafeHwnd();

	m_RecordNum=m_ProcessNum=0;
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDebugPrint_MonitorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDebugPrint_MonitorDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

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
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDebugPrint_MonitorDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

LRESULT CDebugPrint_MonitorDlg::OnSendMessage(WPARAM wParam,LPARAM lParam)
{
    int sig=(int)wParam;
	DebugPrint_Event *pEvent=(DebugPrint_Event*)lParam;
	CString addr1=(CString)(pEvent->addr1);
	CString addr2=(CString)(pEvent->addr2);
	CString addr3=(CString)(pEvent->addr3);
	CString addr4=(CString)(pEvent->addr4);
	CString IP=addr1+"."+addr2+"."+addr3+"."+addr4;

	CString strPID,strPORT,strTIME;

	strPID=(CString)(pEvent->ProcessID);
	strPORT=(CString)(pEvent->port);   
//  strPID.Format("%d",pEvent->ProcessID);
//	strPORT.Format("%d",pEvent->port);
	strTIME=(pEvent->Timestamp).Format("%H:%M:%S"); 

	//如果是插入到记录日志表中
	int nIndex=0;
	if(sig<0)
	{
		nIndex=m_EventListCtr_Record.InsertItem(m_RecordNum,strPID);
        m_EventListCtr_Record.SetItemText(nIndex,0,strPID);
		m_EventListCtr_Record.SetItemText(nIndex,1,pEvent->ProcessName);
		m_EventListCtr_Record.SetItemText(nIndex,2,strTIME);
		m_EventListCtr_Record.SetItemText(nIndex,3,IP);
        m_EventListCtr_Record.SetItemText(nIndex,4,pEvent->Operation);
		m_EventListCtr_Record.SetItemText(nIndex,5,strPORT);
		
		m_RecordNum++;
	}

	//如果是插入到进程视图中
	else
	{
		if(sig>=m_ProcessNum)
		{
			nIndex=m_EventListCtr_Process.InsertItem(m_RecordNum,strPID);
            m_ProcessNum=nIndex;
		    m_EventListCtr_Process.SetItemText(m_ProcessNum,0,strPID);
		    m_EventListCtr_Process.SetItemText(m_ProcessNum,1,pEvent->ProcessName);
		    m_EventListCtr_Process.SetItemText(m_ProcessNum,2,strTIME);
		    m_EventListCtr_Process.SetItemText(m_ProcessNum,3,IP);
		    m_EventListCtr_Process.SetItemText(m_ProcessNum,4,pEvent->Operation);
		    m_EventListCtr_Process.SetItemText(m_ProcessNum,5,strPORT);
		}
		else
		{
			m_EventListCtr_Process.SetItemText(sig,1,pEvent->ProcessName);
		    m_EventListCtr_Process.SetItemText(sig,2,strTIME);
		    m_EventListCtr_Process.SetItemText(sig,3,IP);
		    m_EventListCtr_Process.SetItemText(sig,4,pEvent->Operation);
		    m_EventListCtr_Process.SetItemText(sig,5,strPORT);
		}
		if(sig>=m_ProcessNum)
			m_ProcessNum=sig;
	}

	return 0L;
}

void CDebugPrint_MonitorDlg::OnButtonProcess() 
{
	// TODO: Add your control notification handler code here
	m_EventListCtr_Record.ShowWindow(SW_HIDE);
	m_EventListCtr_Process.ShowWindow(SW_SHOW);
}

void CDebugPrint_MonitorDlg::OnButtonRecord() 
{
	// TODO: Add your control notification handler code here
	m_EventListCtr_Record.ShowWindow(SW_SHOW);
	m_EventListCtr_Process.ShowWindow(SW_HIDE);
}

void CDebugPrint_MonitorDlg::OnButtonInstallDbg() 
{
	// TODO: Add your control notification handler code here
	StartListener();
}

void CDebugPrint_MonitorDlg::OnButtonUninstallDbg() 
{
	// TODO: Add your control notification handler code here
	StopListener();
}
	
EventList *pEvent=new EventList;
void CDebugPrint_MonitorDlg::OnButtonInstallTdi() 
{
	// TODO: Add your control notification handler code here
    
	for(int i=0;i<10;i++)
	{
	char *addr1="0";
	char *addr2="0";
	char *addr3="0";
	char *addr4="0";
	char *ProcessName="UNKNOWN";
	char *operation="UNKNOWN";
	char ProcessID[3];
	char Port[6];
	
	ULONG port=99;
	sprintf(ProcessID, "%d" , i);
	sprintf(Port, "%d" , port);
	
	char *source="woshinidie";
	char str1[15];
	strcpy(str1,source);
	CTime EventTime=NULL;

	pEvent->SendEvent(EventTime,
		    ProcessID,
			ProcessName,
			operation,
			addr1,
			addr2,
			addr3,
			addr4,
			Port
			);
	}
}

void CDebugPrint_MonitorDlg::OnButtonUninstallTdi() 
{
	// TODO: Add your control notification handler code here
	int num = 100;
    char str[25];
    sprintf(str, " %d" , num);
	AfxMessageBox(str);
}
