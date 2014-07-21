// CINSTALLSYS.cpp: implementation of the CCINSTALLSYS class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DebugPrint_Monitor.h"
#include "INSTALLSYS.h"
#include "windows.h"
#include "winsvc.h"
#include "setupapi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CINSTALLSYS::CINSTALLSYS()
{

}

CINSTALLSYS::~CINSTALLSYS()
{

}

CString CINSTALLSYS::Replace(const CString csOriginalWord,const CString csWordDead,const CString csWordLive)
{
      int iDeadLength=csWordDead.GetLength();
      int iLiveLength=csWordLive.GetLength();
      int iBegin,iStart=0;
      CString csResultWord;

      csResultWord.Format("%s",csOriginalWord);
      while( (iBegin=csResultWord.Find(csWordDead,iStart))>=0 ){
        csResultWord.Delete(iBegin,iDeadLength);
        csResultWord.Insert(iBegin,csWordLive);
        iStart=iBegin+iLiveLength;
      }
      return csResultWord;
}
 
BOOL CINSTALLSYS::IsDriverStarted(CString DriverName)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if( hSCManager==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return TRUE;
	}

	SC_HANDLE hDriver = OpenService(hSCManager,DriverName,SERVICE_ALL_ACCESS);
	if( hDriver!=NULL)
	{
		SERVICE_STATUS ss;
	
		if( ControlService(hDriver,SERVICE_CONTROL_INTERROGATE,&ss))
		{
			if( ss.dwCurrentState==SERVICE_STOPPED)
			{
				return FALSE;
			}
			else
			{
				return TRUE;
			}
		}
		return FALSE;
	}

	return FALSE;	
}

BOOL CINSTALLSYS::InstallDriver( CString DriverName, CString DriverFromPath)
{
	if(IsDriverStarted(DriverName)==TRUE)
	{
		AfxMessageBox("驱动已经加载，或无法得到驱动信息，操作已停止");
		return TRUE;
	}

	_TCHAR System32Directory[_MAX_PATH];
	if( 0==GetSystemDirectory(System32Directory,_MAX_PATH))
	{
		AfxMessageBox("Could not find Windows system directory");
		return FALSE;
	}

	/////////////////////////////////////////////////////////////////////////
	// Copy driver .sys file across

	CString DriverFullPath = (CString)System32Directory+"\\Drivers\\"+DriverName+".sys";
	if( 0==CopyFile( DriverFromPath, DriverFullPath, FALSE)) // Overwrite OK
	{
		AfxMessageBox("Could not find Windows system directory");
		return FALSE;
	}
     
	CString SubDriverFromPath=Replace(DriverFromPath,DriverName,"DebugPrt");
	CString SubDriverFullPath=(CString)System32Directory+"\\Drivers\\"+"DebugPrt.sys";
	if(0==CopyFile(SubDriverFromPath,SubDriverFullPath,FALSE))
	{
		AfxMessageBox("Could not find Windows system directory");
		return FALSE;
	}

	//创建服务
	if(!CreateDriverService(DriverName))
		return FALSE;

	if(!RegeditKey(DriverName) || !RegeditKey(TEXT("DebugPrt")))
		return FALSE;

	//开始服务
    if(!StartDriverService(DriverName))
		return FALSE;

	AfxMessageBox("传输驱动程序安装成功！");
	return TRUE;
}

BOOL CINSTALLSYS::CreateDriverService( CString DriverName)
{
	SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if(hSCManager==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return FALSE;
	}

	//创建主驱动服务
	SC_HANDLE hDriver = CreateService(hSCManager,DriverName,DriverName,SERVICE_ALL_ACCESS,
							SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_IGNORE,
							DriverName,NULL,NULL,NULL,NULL,NULL);
	if( hDriver==NULL)
	{
		AfxMessageBox("Could not install driver with Service Control Manager");
		CloseServiceHandle(hSCManager);
		return FALSE;
	}
	CloseServiceHandle(hDriver);
	CloseServiceHandle(hSCManager);

	//创建次驱动服务
	SC_HANDLE hSCManager2 = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if(hSCManager2==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return FALSE;
	}
	SC_HANDLE hDriver2 = CreateService(hSCManager2,TEXT("DebugPrt"),TEXT("DebugPrt"),SERVICE_ALL_ACCESS,
							SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_IGNORE,
							TEXT("DebugPrt"),NULL,NULL,NULL,NULL,NULL);
	if( hDriver2==NULL)
	{
		AfxMessageBox("Could not install TDI driver with Service Control Manager");
		CloseServiceHandle(hSCManager2);
		return FALSE;
	}
    CloseServiceHandle(hDriver2);
	CloseServiceHandle(hSCManager2);
	return TRUE;
}

BOOL CINSTALLSYS::StartDriverService(CString DriverName)
{
	//首先启动DebugPrt驱动程序
	
	SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if( hSCManager==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return FALSE;
	}

	SC_HANDLE hDriver = OpenService(hSCManager,"DebugPrt",SERVICE_ALL_ACCESS);
	if( hDriver==NULL)
	{
		AfxMessageBox("Could not open driver service");
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	if( !StartService(hDriver,0,NULL) 
		&& GetLastError() != ERROR_SERVICE_ALREADY_RUNNING
		&& GetLastError() != ERROR_IO_PENDING)
	{
		AfxMessageBox("Could not start DebugPrt driver");		
		CloseServiceHandle(hSCManager);
		CloseServiceHandle(hDriver);
		//return FALSE;
	}
	CloseServiceHandle(hDriver);
	CloseServiceHandle(hSCManager);

	//然后打开DebugPrt设备
	//StartListener();

	//然后启动TDI驱动程序
	SC_HANDLE hSCManager2 = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if( hSCManager2==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return FALSE;
	}

	SC_HANDLE hDriver2 = OpenService(hSCManager2,DriverName,SERVICE_ALL_ACCESS);
	if( hDriver2==NULL)
	{
		AfxMessageBox("Could not open driver service");
		CloseServiceHandle(hSCManager2);
		return FALSE;
	}

	if( !StartService(hDriver2,0,NULL) 
		&& GetLastError() != ERROR_SERVICE_ALREADY_RUNNING
		&& GetLastError() != ERROR_IO_PENDING)
	{
		AfxMessageBox("Could not start TDI driver");		
		CloseServiceHandle(hSCManager2);
		CloseServiceHandle(hDriver2);
		return FALSE;
	}
	CloseServiceHandle(hDriver2);
	CloseServiceHandle(hSCManager2);

	return TRUE;
}

void CINSTALLSYS::StartDriver()
{
	StartListener();
}

void CINSTALLSYS::StopDriver()
{
	StopListener();
}

BOOL CINSTALLSYS::UnInstallDriver( CString DriverName)
{
	HKEY hKey;
	if((RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName,0,KEY_WRITE,&hKey))
		==ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		if(RegDeleteKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName+"\\Enum")
		!= ERROR_SUCCESS)	
		{		
			AfxMessageBox("Could not delete Parameters registry key");		
			return FALSE;	
		}
		
		if(RegDeleteKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName+"\\Parameters")
		!= ERROR_SUCCESS)	
		{		
			AfxMessageBox("Could not delete Parameters registry key");		
			return FALSE;	
		}
		if(RegDeleteKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName+"\\Security")
		!= ERROR_SUCCESS)	
		{		
			AfxMessageBox("Could not delete Security registry key");		
			return FALSE;	
		}
		if(RegDeleteKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName)
		!= ERROR_SUCCESS)	
		{		
			AfxMessageBox("Could not delete Driver registry key");		
			return FALSE;	
		}		
	}

	else
		return FALSE;

	if((RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\System\\"+DriverName,0,KEY_WRITE,&hKey))
		==ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		if(RegDeleteKey(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\System\\"+DriverName)
		!= ERROR_SUCCESS)	
		{		
			AfxMessageBox("Could not delete EventLog registry key");		
			return FALSE;	
		}
	}
	else
		return FALSE;

	_TCHAR System32Directory[_MAX_PATH];
	if( 0==GetSystemDirectory(System32Directory,_MAX_PATH))
	{
		AfxMessageBox("Could not delete the sys file");
		return FALSE;
	}
	DeleteFile( (CString)System32Directory+"\\Drivers\\"+DriverName+".sys");
	DeleteFile( (CString)System32Directory+"\\Drivers\\DebugPrt.sys");

	return TRUE;
}

BOOL CINSTALLSYS::RegeditKey(CString DriverName)
{
	HKEY mru;
	DWORD disposition;
	if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName,
						0, "", 0, KEY_ALL_ACCESS, NULL, &mru, &disposition)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry key");
		return FALSE;
	}
	// Delete ImagePath
	RegDeleteValue(mru,"ImagePath");
	// Delete DisplayName
	RegDeleteValue(mru,"DisplayName");
	// ErrorControl
	DWORD dwRegValue = SERVICE_ERROR_NORMAL;
	if( RegSetValueEx(mru,"ErrorControl",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value ErrorControl");
		return FALSE;
	}
	// Start
	dwRegValue = SERVICE_AUTO_START;
	if( RegSetValueEx(mru,"Start",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value Start");
		return FALSE;
	}
	// Type
	dwRegValue = SERVICE_KERNEL_DRIVER;
	if( RegSetValueEx(mru,"Type",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value Type");
		return FALSE;
	}
	// DependOnGroup
	_TCHAR DependOnGroup[] = "Parallel arbitrator\0\0";
	if( RegSetValueEx(mru,"DependOnGroup",0,REG_MULTI_SZ,(CONST BYTE*)&DependOnGroup,strlen(DependOnGroup)+2)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value DependOnGroup");
		return FALSE;
	}
	// DependOnService
	_TCHAR DependOnService[] = "parport\0\0";
	if( RegSetValueEx(mru,"DependOnService",0,REG_MULTI_SZ,(CONST BYTE*)&DependOnService,strlen(DependOnService)+2)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value DependOnService");
		return FALSE;
	}
	RegCloseKey(mru);

	/////////////////////////////////////////////////////////////////////////
	// Create/Open driver\Parameters registry key and set its values

	if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName+"\\Parameters",
						0, "", 0, KEY_ALL_ACCESS, NULL, &mru, &disposition)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver\\Parameters registry key");
		return FALSE;
	}
	// EventLogLevel
	dwRegValue = 1;
	if( RegSetValueEx(mru,"EventLogLevel",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver\\Parameters registry value EventLogLevel");
		return FALSE;
	}
	// Default or No Name
	CString DefaultName = DriverName;
	int DeviceNameLen = DefaultName.GetLength()+1;
	LPTSTR lpDefaultName = DefaultName.GetBuffer(DeviceNameLen);
	if( RegSetValueEx(mru,"",0,REG_SZ,(CONST BYTE*)lpDefaultName,DeviceNameLen)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver\\Parameters default registry value");
		return FALSE;
	}
	
	RegCloseKey(mru);

	/////////////////////////////////////////////////////////////////////////
	// Open EventLog\System registry key and set its values

	if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\System",
						0, "", 0, KEY_ALL_ACCESS, NULL, &mru, &disposition)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not open EventLog\\System registry key");
		return FALSE;
	}
	// get Sources size
	DWORD DataSize = 0;
	DWORD Type;
	if( RegQueryValueEx(mru,"Sources",NULL,&Type,NULL,&DataSize)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not read size of EventLog\\System registry value Sources");
		return FALSE;
	}
	// read Sources
	int DriverNameLen = strlen(DriverName);
	DataSize += DriverNameLen+1;
	LPTSTR Sources = new _TCHAR[DataSize];
	if( RegQueryValueEx(mru,"Sources",NULL,&Type,(LPBYTE)Sources,&DataSize)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not read EventLog\\System registry value Sources");
		return FALSE;
	}
	// If driver not there, add and write

	if( FindInMultiSz(Sources,DataSize,lpDefaultName)==-1)
	{
		strcpy(Sources+DataSize-1,DriverName);
		DataSize += DriverNameLen;
		*(Sources+DataSize) = '\0';

		if( RegSetValueEx(mru,"Sources",0,REG_MULTI_SZ,(CONST BYTE*)Sources,DataSize)
			!= ERROR_SUCCESS)
		{
			AfxMessageBox("Could not create driver registry value Sources");
			return FALSE;
		}
	}

	//DriverName.ReleaseBuffer(0);

	/////////////////////////////////////////////////////////////////////////
	// Create/Open EventLog\System\driver registry key and set its values

	if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\System\\"+DriverName,
						0, "", 0, KEY_ALL_ACCESS, NULL, &mru, &disposition)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create EventLog\\System\\driver registry key");
		return FALSE;
	}
	// TypesSupported
	dwRegValue = 7;
	if( RegSetValueEx(mru,"TypesSupported",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create EventLog\\System\\driver registry value TypesSupported");
		return FALSE;
	}
	// EventMessageFile
	//CString DefaultName = DriverName;
	//int DeviceNameLen = DefaultName.GetLength()+1;
	lpDefaultName = DefaultName.GetBuffer(DeviceNameLen);

	char EventMessageFile[1024]="%SystemRoot%\\System32\\IoLogMsg.dll;%SystemRoot%\\System32\\Drivers\\";
	strcat(EventMessageFile,lpDefaultName);
	strcat(EventMessageFile,".sys");
	//LPTSTR EventMessageFile ="%SystemRoot%\\System32\\IoLogMsg.dll;%SystemRoot%\\System32\\Drivers\\"+DriverName+".sys";
	if( RegSetValueEx(mru,"EventMessageFile",0,REG_EXPAND_SZ,(CONST BYTE*)EventMessageFile,strlen(EventMessageFile)+1)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create EventLog\\System\\driver registry value EventMessageFile");
		return FALSE;
	}
	RegCloseKey(mru);

	return TRUE;
}

int CINSTALLSYS::FindInMultiSz(LPTSTR MultiSz, int MultiSzLen, LPTSTR Match)
{
	int MatchLen = strlen(Match);
	_TCHAR FirstChar = *Match;
	for(int i=0;i<MultiSzLen-MatchLen;i++)
	{
		if( *MultiSz++ == FirstChar)
		{
			BOOL Found = TRUE;
			LPTSTR Try = MultiSz;
			for(int j=1;j<=MatchLen;j++)
				if( *Try++ != Match[j])
				{
					Found = FALSE;
					break;
				}
			if( Found)
				return i;
		}
	}
	return -1;

}

