// CINSTALLSYS.cpp: implementation of the CCINSTALLSYS class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DebugPrint_Monitor.h"
#include "INSTALLSYS.h"
#include "windows.h"
#include "winsvc.h"

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

BOOL CINSTALLSYS::IsDriverStarted(CString DriverName)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if( hSCManager==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return TRUE;
	}

	/////////////////////////////////////////////////////////////////////////
	// If driver is running, stop it

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
		return TRUE;
	}

	return FALSE;	
}

void CINSTALLSYS::InstallDriver( CString DriverName, CString DriverFromPath)
{
	/////////////////////////////////////////////////////////////////////////
	// Get System32 directory

	if(IsDriverStarted(DriverName)==TRUE)
	{
		AfxMessageBox("驱动已经加载，或无法得到驱动信息，操作已停止");
		return;
	}

	_TCHAR System32Directory[_MAX_PATH];
	if( 0==GetSystemDirectory(System32Directory,_MAX_PATH))
	{
		AfxMessageBox("Could not find Windows system directory");
		return;
	}

	/////////////////////////////////////////////////////////////////////////
	// Copy driver .sys file across

	CString DriverFullPath = (CString)System32Directory+"\\Drivers\\"+DriverName+".sys";
	if( 0==CopyFile( DriverFromPath, DriverFullPath, FALSE)) // Overwrite OK
	{
		CString Msg;
		Msg.Format("Could not copy %s to %s", DriverFullPath, DriverName);
		AfxMessageBox(Msg);
		return;
	}

	/////////////////////////////////////////////////////////////////////////
	// Create driver (or stop existing driver)

	if( !CreateDriver( DriverName, DriverFullPath))
		return;

	/////////////////////////////////////////////////////////////////////////
	// Create/Open driver registry key and set its values
	//	Overwrite registry values written in driver creation

	HKEY mru;
	DWORD disposition;
	if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName,
						0, "", 0, KEY_ALL_ACCESS, NULL, &mru, &disposition)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry key");
		return;
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
		return;
	}
	// Start
	dwRegValue = SERVICE_AUTO_START;
	if( RegSetValueEx(mru,"Start",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value Start");
		return;
	}
	// Type
	dwRegValue = SERVICE_KERNEL_DRIVER;
	if( RegSetValueEx(mru,"Type",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value Type");
		return;
	}
	// DependOnGroup
	_TCHAR DependOnGroup[] = "Parallel arbitrator\0\0";
	if( RegSetValueEx(mru,"DependOnGroup",0,REG_MULTI_SZ,(CONST BYTE*)&DependOnGroup,strlen(DependOnGroup)+2)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value DependOnGroup");
		return;
	}
	// DependOnService
	_TCHAR DependOnService[] = "parport\0\0";
	if( RegSetValueEx(mru,"DependOnService",0,REG_MULTI_SZ,(CONST BYTE*)&DependOnService,strlen(DependOnService)+2)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver registry value DependOnService");
		return;
	}
	RegCloseKey(mru);

	/////////////////////////////////////////////////////////////////////////
	// Create/Open driver\Parameters registry key and set its values

	if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\"+DriverName+"\\Parameters",
						0, "", 0, KEY_ALL_ACCESS, NULL, &mru, &disposition)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver\\Parameters registry key");
		return;
	}
	// EventLogLevel
	dwRegValue = 1;
	if( RegSetValueEx(mru,"EventLogLevel",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver\\Parameters registry value EventLogLevel");
		return;
	}
	// Default or No Name
	CString DefaultName = DriverName;
	int DeviceNameLen = DefaultName.GetLength()+1;
	LPTSTR lpDefaultName = DefaultName.GetBuffer(DeviceNameLen);
	if( RegSetValueEx(mru,"",0,REG_SZ,(CONST BYTE*)lpDefaultName,DeviceNameLen)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create driver\\Parameters default registry value");
		return;
	}
	
	RegCloseKey(mru);

	/////////////////////////////////////////////////////////////////////////
	// Open EventLog\System registry key and set its values

	if( RegCreateKeyEx( HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\System",
						0, "", 0, KEY_ALL_ACCESS, NULL, &mru, &disposition)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not open EventLog\\System registry key");
		return;
	}
	// get Sources size
	DWORD DataSize = 0;
	DWORD Type;
	if( RegQueryValueEx(mru,"Sources",NULL,&Type,NULL,&DataSize)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not read size of EventLog\\System registry value Sources");
		return;
	}
	// read Sources
	int DriverNameLen = strlen(DriverName);
	DataSize += DriverNameLen+1;
	LPTSTR Sources = new _TCHAR[DataSize];
	if( RegQueryValueEx(mru,"Sources",NULL,&Type,(LPBYTE)Sources,&DataSize)
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not read EventLog\\System registry value Sources");
		return;
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
			return;
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
		return;
	}
	// TypesSupported
	dwRegValue = 7;
	if( RegSetValueEx(mru,"TypesSupported",0,REG_DWORD,(CONST BYTE*)&dwRegValue,sizeof(DWORD))
		!= ERROR_SUCCESS)
	{
		AfxMessageBox("Could not create EventLog\\System\\driver registry value TypesSupported");
		return;
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
		return;
	}
	RegCloseKey(mru);

	//DefaultName.ReleaseBuffer(0);

	/////////////////////////////////////////////////////////////////////////
	// Start driver service

	if( !StartDriver(DriverName))
		return;
}


/////////////////////////////////////////////////////////////////////////////

BOOL CINSTALLSYS::CreateDriver( CString DriverName, CString FullDriver)
{
	/////////////////////////////////////////////////////////////////////////
	// Open service control manager

	SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if( hSCManager==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return FALSE;
	}

	/////////////////////////////////////////////////////////////////////////
	// If driver is running, stop it

	SC_HANDLE hDriver = OpenService(hSCManager,DriverName,SERVICE_ALL_ACCESS);
	if( hDriver!=NULL)
	{
		SERVICE_STATUS ss;
	
		if( ControlService(hDriver,SERVICE_CONTROL_INTERROGATE,&ss))
		{
			if( ss.dwCurrentState!=SERVICE_STOPPED)
			{
				if( !ControlService(hDriver,SERVICE_CONTROL_STOP,&ss))
				{
					AfxMessageBox("Could not stop driver");
					CloseServiceHandle(hSCManager);
					CloseServiceHandle(hDriver);
					return FALSE;
				}
				// Give it 10 seconds to stop
				BOOL Stopped = FALSE;
				for(int seconds=0;seconds<1000;seconds++)
				{
					Sleep(1000);
					if( ControlService(hDriver,SERVICE_CONTROL_INTERROGATE,&ss) &&
						ss.dwCurrentState==SERVICE_STOPPED)
					{
						Stopped = TRUE;
						break;
					}
				}
				if( !Stopped)
				{
					AfxMessageBox("Could not stop driver");
					CloseServiceHandle(hSCManager);
					CloseServiceHandle(hDriver);
					return FALSE;
				}
			}
			CloseServiceHandle(hDriver);
		}
		return TRUE;	
	}
	/////////////////////////////////////////////////////////////////////////
	// Create driver service

	hDriver = CreateService(hSCManager,DriverName,DriverName,SERVICE_ALL_ACCESS,
							SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,
							DriverName,NULL,NULL,"parport\0\0",NULL,NULL);
	if( hDriver==NULL)
	{
		AfxMessageBox("Could not install driver with Service Control Manager");
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	/////////////////////////////////////////////////////////////////////////
	CloseServiceHandle(hSCManager);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

BOOL CINSTALLSYS::StartDriver(CString DriverName)
{
	/////////////////////////////////////////////////////////////////////////
	// Open service control manager

	SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
	if( hSCManager==NULL)
	{
		AfxMessageBox("Could not open Service Control Manager");
		return FALSE;
	}

	/////////////////////////////////////////////////////////////////////////
	// Driver isn't there

	SC_HANDLE hDriver = OpenService(hSCManager,DriverName,SERVICE_ALL_ACCESS);
	if( hDriver==NULL)
	{
		AfxMessageBox("Could not open driver service");
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	SERVICE_STATUS ss;
	if( !ControlService(hDriver,SERVICE_CONTROL_INTERROGATE,&ss) ||
		ss.dwCurrentState!=SERVICE_STOPPED)
	{
		if(ss.dwCurrentState!=SERVICE_STOPPED)
			AfxMessageBox("SERVICE IS NOT STOPPED");

		switch(GetLastError())
		{
		case ERROR_ACCESS_DENIED:
			AfxMessageBox("ERROR_ACCESS_DENIED");
			break;
		case ERROR_DEPENDENT_SERVICES_RUNNING:
			AfxMessageBox("ERROR_DEPENDENT_SERVICES_RUNNING");
			break;
		case ERROR_INVALID_HANDLE:
			AfxMessageBox("ERROR_INVALID_HANDLE");
			break;
		case ERROR_INVALID_SERVICE_CONTROL:
			AfxMessageBox("ERROR_INVALID_SERVICE_CONTROL");
			break;
		case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
			AfxMessageBox("ERROR_SERVICE_CANNOT_ACCEPT_CTRL");
			break;
		case ERROR_SERVICE_NOT_ACTIVE:
			AfxMessageBox("ERROR_SERVICE_NOT_ACTIVE");
			break;
		case ERROR_SERVICE_REQUEST_TIMEOUT:
			AfxMessageBox("ERROR_SERVICE_REQUEST_TIMEOUT");
			break;
		case ERROR_SHUTDOWN_IN_PROGRESS:
			AfxMessageBox("ERROR_SHUTDOWN_IN_PROGRESS");
			break;
		default:
			break;
		}

		AfxMessageBox("Could not interrogate driver service");
		//CloseServiceHandle(hSCManager);
		//CloseServiceHandle(hDriver);
		//return FALSE;
	}
	if( !StartService(hDriver,0,NULL))
	{
		AfxMessageBox("Could not start driver");
		CloseServiceHandle(hSCManager);
		CloseServiceHandle(hDriver);
		return FALSE;
	}
	// Give it 10 seconds to start
	BOOL Started = FALSE;
	for(int seconds=0;seconds<10;seconds++)
	{
		Sleep(1000);
		if( ControlService(hDriver,SERVICE_CONTROL_INTERROGATE,&ss) &&
			ss.dwCurrentState==SERVICE_RUNNING)
		{
			Started = TRUE;
			break;
		}
	}
	if( !Started)
	{
		AfxMessageBox("Could not start driver");
		CloseServiceHandle(hSCManager);
		CloseServiceHandle(hDriver);
		return FALSE;
	}
	CloseServiceHandle(hDriver);
	CloseServiceHandle(hSCManager);
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
//	Try to find Match in MultiSz, including Match's terminating \0

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
