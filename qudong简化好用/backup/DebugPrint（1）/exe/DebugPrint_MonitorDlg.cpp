// DebugPrint_MonitorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "DebugPrint_Monitor.h"
#include "DebugPrint_MonitorDlg.h"
#include "listener.h"
#include "INSTALLSYS.h"
  


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
    m_EventListCtr_Process.InsertColumn(0,"进程号",LVCFMT_CENTER,60);
	m_EventListCtr_Process.InsertColumn(1,"进程名称",LVCFMT_CENTER,100);
	m_EventListCtr_Process.InsertColumn(2,"时间",LVCFMT_CENTER,80);
	m_EventListCtr_Process.InsertColumn(3,"IP地址",LVCFMT_CENTER,150);
	m_EventListCtr_Process.InsertColumn(4,"远程端口号",LVCFMT_CENTER,80);
	m_EventListCtr_Process.InsertColumn(5,"本地端口号",LVCFMT_CENTER,80);
	m_EventListCtr_Process.InsertColumn(6,"操作",LVCFMT_LEFT,160);	
	m_EventListCtr_Process.InsertColumn(7,"是否成功",LVCFMT_CENTER,80);
	m_EventListCtr_Process.SetExtendedStyle(LVS_EX_GRIDLINES);
    ::SendMessage(m_EventListCtr_Process.m_hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE,
      LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	m_EventListCtr_Record.InsertColumn(0,"进程号",LVCFMT_CENTER,60);
	m_EventListCtr_Record.InsertColumn(1,"进程名称",LVCFMT_CENTER,100);
	m_EventListCtr_Record.InsertColumn(2,"时间",LVCFMT_CENTER,80);
	m_EventListCtr_Record.InsertColumn(3,"IP地址",LVCFMT_CENTER,150);
	m_EventListCtr_Record.InsertColumn(4,"远程端口号",LVCFMT_CENTER,80);
	m_EventListCtr_Record.InsertColumn(5,"本地端口号",LVCFMT_CENTER,80);
	m_EventListCtr_Record.InsertColumn(6,"操作",LVCFMT_LEFT,160);	
	m_EventListCtr_Record.InsertColumn(7,"是否成功",LVCFMT_CENTER,80);
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

	CString strPID,strRemotePORT,strLocalPORT,strTIME;

	strPID=(CString)(pEvent->ProcessID);
	strRemotePORT=(CString)(pEvent->RemotePort);  
	strLocalPORT=(CString)(pEvent->LocalPort);
	CString result=(CString)(pEvent->SuccOrFail);
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
        m_EventListCtr_Record.SetItemText(nIndex,4,strRemotePORT);
		m_EventListCtr_Record.SetItemText(nIndex,5,strLocalPORT);
		m_EventListCtr_Record.SetItemText(nIndex,6,pEvent->Operation);
        m_EventListCtr_Record.SetItemText(nIndex,7,result);
		
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
		    m_EventListCtr_Process.SetItemText(m_ProcessNum,4,strRemotePORT);
			m_EventListCtr_Process.SetItemText(m_ProcessNum,5,strLocalPORT);
			m_EventListCtr_Process.SetItemText(m_ProcessNum,6,pEvent->Operation);		    
			m_EventListCtr_Process.SetItemText(m_ProcessNum,7,result);
		}
		else
		{
			m_EventListCtr_Process.SetItemText(sig,1,pEvent->ProcessName);
		    m_EventListCtr_Process.SetItemText(sig,2,strTIME);
		    m_EventListCtr_Process.SetItemText(sig,3,IP);
			m_EventListCtr_Process.SetItemText(sig,4,strRemotePORT);
			m_EventListCtr_Process.SetItemText(sig,5,strLocalPORT);
		    m_EventListCtr_Process.SetItemText(sig,6,pEvent->Operation);		    
			m_EventListCtr_Process.SetItemText(sig,7,result);
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
	
//EventList *pEvent=new EventList;
void CDebugPrint_MonitorDlg::OnButtonInstallTdi() 
{
	// TODO: Add your control notification handler code here
    /*
	for(int i=0;i<10;i++)
	{
	char *addr1="0";
	char *addr2="0";
	char *addr3="0";
	char *addr4="0";
	char *ProcessName="UNKNOWN";
	char *operation="UNKNOWN";
	char *result="SUCCESS";
	char ProcessID[3];
	char Port[6];
	
	ULONG port=99;
	sprintf(ProcessID, "%d" , i);
	sprintf(Port, "%d" , port);
	
	char *source="woshinidie";
	char *destiny="nishiwoer";
	char str1[15];
	strcpy(str1,source);
	AfxMessageBox((CString)str1);
	strcpy(str1,destiny);
	AfxMessageBox((CString)str1);
	CTime EventTime=NULL;

	pEvent->SendEvent(EventTime,
		    ProcessID,
			ProcessName,
			operation,
			addr1,
			addr2,
			addr3,
			addr4,
			Port,
			result
			);
	}
*/
	static char BASED_CODE szFilter[]="System Files(*.sys)|*.sys|System98 Files(*.vxd)|*.vxd|";
	CString strPathName;
	CString strPath;
	CString strName;
	CFileDialog dlg(TRUE,"SYS",NULL,OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,szFilter,this);
	if(dlg.DoModal()==IDOK)
	{
		strPathName=(CString)dlg.GetPathName();
		strName=(CString)dlg.GetFileName();
	}
	//AfxMessageBox(strName);
	char filename[100];
	int strNameLen = strName.GetLength()+1;
	char *lpStrName= strName.GetBuffer(strNameLen);
	strncpy(filename,lpStrName,strNameLen-5);
	filename[strNameLen-5]='\0';
	CINSTALLSYS install;
	install.InstallDriver((CString)filename,strPathName);
}

void CDebugPrint_MonitorDlg::OnButtonUninstallTdi() 
{

	// TODO: Add your control notification handler code here 
}
 
/*
STDMETHODIMP  CDebugPrint_MonitorDlg::Disable(BSTR  DriverID,  long  *pVal)  
{  
           BOOL  ShowHidden  =  0;  
           HDEVINFO  hDevInfo  =  0;  
           long  len;  
           //init  the  value  
           TIndex  =  -1;  
           len  =  wcstombs(NULL,DriverID,wcslen(DriverID));  
           len  =  len  +  1;  
           DrvID  =  (char  *)malloc(len);  
           memset(DrvID,0,len+1);  
           wcstombs(DrvID,DriverID,wcslen(DriverID));  
 
 
           if  (INVALID_HANDLE_VALUE  ==  (hDevInfo  =    
                               SetupDiGetClassDevs(NULL,NULL,NULL,  
                               DIGCF_PRESENT  |DIGCF_ALLCLASSES)))  
                                                 
               {  
 
                       *pVal  =  -1;  
                                   return  S_OK;  
               }  
//get  the  index  of  drv  in  the  set  
           EnumAddDevices(ShowHidden,hDevInfo);  
 
//disable  the  drv              
               
//            if  ((IsDisableable(TIndex,hDevInfo))&&(!(TIndex==-1)))  
           if  (!IsDisabled(TIndex,hDevInfo))    
                       if  (IsDisableable(TIndex,hDevInfo))  
                                   if  (StateChange(DICS_DISABLE,TIndex,hDevInfo))  
                                               *pVal  =  0;  
                                   else  
                                               *pVal  =  -1;                                      
                       else  
                                   *pVal  =  1;                                      
           else    
                       *pVal  =  0;  
 
           if(hDevInfo)  
                       SetupDiDestroyDeviceInfoList(hDevInfo);  
           return  S_OK;  
}  
 
BOOL  CDebugPrint_MonitorDlg::EnumAddDevices(BOOL  ShowHidden,  HDEVINFO  hDevInfo)  
{  
           DWORD  i,  Status,  Problem;  
 
       LPTSTR  IOName=NULL;  
           DWORD  len=0;  
       SP_DEVINFO_DATA  DeviceInfoData  =  {sizeof(SP_DEVINFO_DATA)};  
             
     
 
       //  
       //  Enumerate  though  all  the  devices.  
       //  
       for  (i=0;SetupDiEnumDeviceInfo(hDevInfo,i,&DeviceInfoData);i++)  
       {  
               //  
               //  Should  we  display  this  device,  or  move  onto  the  next  one.  
               //  
               if  (CR_SUCCESS  !=  CM_Get_DevNode_Status(&Status,  &Problem,  
                                       DeviceInfoData.DevInst,0))  
               {  
                         
                       continue;  
               }  
 
               if  (!(ShowHidden    |  |  !((Status  &  DN_NO_SHOW_IN_DM)    |  |    
                       IsClassHidden(&DeviceInfoData.ClassGuid))))  
                       continue;  
 
 
 
               //  
               //  Get  a  friendly  name  for  the  device.  
               //  
                                     
               ConstructDeviceName(hDevInfo,&DeviceInfoData,  
                                                                                   &IOName,  
                                                                                   (PULONG)&len);  
                       if  (strcmp(IOName,DrvID)  ==  0)  
                       {  
                                   TIndex  =  i;  
                                   return  TRUE;  
                       }  
           }  
           return  TRUE;  
       
}  
  
BOOL  CDebugPrint_MonitorDlg::IsClassHidden(GUID  *ClassGuid)  
{  
           BOOL  bHidden  =  FALSE;  
       HKEY  hKeyClass;  
 
       //  
       //  If  the  devices  class  has  the  NoD isplayClass  value  then  
       //  don't  display  this  device.  
       //  
       if  (hKeyClass  =  SetupDiOpenClassRegKey(ClassGuid,KEY_READ))  
       {  
               bHidden  =  (RegQueryValueEx(hKeyClass,    
                       REGSTR_VAL_NODISPLAYCLASS,    
                       NULL,  NULL,  NULL,  NULL)  ==  ERROR_SUCCESS);  
               RegCloseKey(hKeyClass);  
       }                                                                    
 
       return  bHidden;  
}  
 
BOOL  CDebugPrint_MonitorDlg::ConstructDeviceName(HDEVINFO  DeviceInfoSet,  PSP_DEVINFO_DATA  DeviceInfoData,  PVOID  Buffer,  PULONG  Length)  
{  
           if  (!GetRegistryProperty(DeviceInfoSet,  
               DeviceInfoData,  
               SPDRP_DRIVER  ,  
               Buffer,  
               Length))  
       {  
               if  (!GetRegistryProperty(DeviceInfoSet,  
                       DeviceInfoData,  
                       SPDRP_DEVICEDESC  ,  
                       Buffer,  
                       Length))  
               {  
                       if  (!GetRegistryProperty(DeviceInfoSet,  
                               DeviceInfoData,  
                               SPDRP_CLASS  ,  
                               Buffer,  
                               Length))  
                       {  
                               if  (!GetRegistryProperty(DeviceInfoSet,  
                                       DeviceInfoData,  
                                       SPDRP_CLASSGUID  ,  
                                       Buffer,  
                                       Length))  
                               {  
                                         *Length  =  (_tcslen(UnknownDevice)+1)*sizeof(TCHAR);  
                                         Buffer  =(char  *)malloc(*Length);  
                                         _tcscpy(*(LPTSTR  *)Buffer,UnknownDevice);  
                               }  
                       }  
               }  
 
       }  
 
         
           return  TRUE;  
}  
 
BOOL  CDebugPrint_MonitorDlg::GetRegistryProperty(HDEVINFO  DeviceInfoSet,  PSP_DEVINFO_DATA  DeviceInfoData,  ULONG  Property,  PVOID  Buffer,  PULONG  Length)  
{  
           while  (!SetupDiGetDeviceRegistryProperty(DeviceInfoSet,  
               DeviceInfoData,  
               Property,  
               NULL,  
               (BYTE  *)*(TCHAR  **)Buffer,  
               *Length,  
               Length  
               ))  
       {  
 
               if  (GetLastError()  ==  ERROR_INSUFFICIENT_BUFFER)  
               {  
                       //  
                       //  We  need  to  change  the  buffer  size.  
                       //  
                       if  (*(LPTSTR  *)Buffer)    
                               LocalFree(*(LPTSTR  *)Buffer);  
                       *(LPTSTR  *)Buffer  =  (PCHAR)LocalAlloc(LPTR,*Length);  
               }  
               else  
               {  
                       //  
                       //  Unknown  Failure.  
                       //  
                 
                       return  FALSE;  
               }                          
       }  
 
       return  (*(LPTSTR  *)Buffer)[0];  
 
}  
 
BOOL  CDebugPrint_MonitorDlg::StateChange(DWORD  NewState,  DWORD  SelectedItem,  HDEVINFO  hDevInfo)  
{  
           SP_PROPCHANGE_PARAMS  PropChangeParams  =  {sizeof(SP_CLASSINSTALL_HEADER)};  
       SP_DEVINFO_DATA  DeviceInfoData  =  {sizeof(SP_DEVINFO_DATA)};  
         
       //  
       //  Get  a  handle  to  the  Selected  Item.  
       //  
       if  (!SetupDiEnumDeviceInfo(hDevInfo,SelectedItem,&DeviceInfoData))  
       {  
 
               return  FALSE;  
       }  
 
       //  
       //  Set  the  PropChangeParams  structure.  
       //  
       PropChangeParams.ClassInstallHeader.InstallFunction  =  DIF_PROPERTYCHANGE;  
       PropChangeParams.Scope  =  DICS_FLAG_GLOBAL;  
       PropChangeParams.StateChange  =  NewState;    
 
       if  (!SetupDiSetClassInstallParams(hDevInfo,  
               &DeviceInfoData,  
               (SP_CLASSINSTALL_HEADER  *)&PropChangeParams,  
               sizeof(PropChangeParams)))  
       {  
 
         
               return  FALSE;  
       }  
 
       //  
       //  Call  the  ClassInstaller  and  perform  the  change.  
       //  
       if  (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,  
               hDevInfo,  
               &DeviceInfoData))  
       {  
 
                 
               return  TRUE;  
       }  
 
         
       return  TRUE;  
}  
 
BOOL  CDebugPrint_MonitorDlg::IsDisabled(DWORD  SelectedItem,  HDEVINFO  hDevInfo)  
{  
           SP_DEVINFO_DATA  DeviceInfoData  =  {sizeof(SP_DEVINFO_DATA)};  
       DWORD  Status,  Problem;  
 
       //  
       //  Get  a  handle  to  the  Selected  Item.  
       //  
       if  (!SetupDiEnumDeviceInfo(hDevInfo,SelectedItem,&DeviceInfoData))  
       {  
 
               return  FALSE;  
       }  
 
       if  (CR_SUCCESS  !=  CM_Get_DevNode_Status(&Status,  &Problem,  
                               DeviceInfoData.DevInst,0))  
       {  
 
               return  FALSE;  
       }  
 
       return  ((Status  &  DN_HAS_PROBLEM)  &&  (CM_PROB_DISABLED  ==  Problem))  ;  
}  
 
BOOL  CDebugPrint_MonitorDlg::IsDisableable(DWORD  SelectedItem,  HDEVINFO  hDevInfo)  
{  
           SP_DEVINFO_DATA  DeviceInfoData  =  {sizeof(SP_DEVINFO_DATA)};  
       DWORD  Status,  Problem;  
 
       //  
       //  Get  a  handle  to  the  Selected  Item.  
       //  
       if  (!SetupDiEnumDeviceInfo(hDevInfo,SelectedItem,&DeviceInfoData))  
       {  
 
               return  FALSE;  
       }  
 
       if  (CR_SUCCESS  !=  CM_Get_DevNode_Status(&Status,  &Problem,  
                               DeviceInfoData.DevInst,0))  
       {  
 
               return  FALSE;  
       }  
 
       return  ((Status  &  DN_DISABLEABLE)  &&    
               (CM_PROB_HARDWARE_DISABLED  !=  Problem));  

}
*/