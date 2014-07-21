#include "stdafx.h"
#include "afxmt.h"
#include "setupapi.h"

#include "DebugPrint_Monitor.h"

#include "initguid.h"
#include "..\DbgPrtGUID.h"

#include "Listener.h"


typedef struct _LISTENER_INFO
{
	HANDLE DebugPrintDriver;
	bool KeepGoing;
} LISTENER_INFO, *PLISTENER_INFO;

typedef short CSHORT;

typedef struct _TIME_FIELDS {
    CSHORT Year;        // range [1601...]
    CSHORT Month;       // range [1..12]
    CSHORT Day;         // range [1..31]
    CSHORT Hour;        // range [0..23]
    CSHORT Minute;      // range [0..59]
    CSHORT Second;      // range [0..59]
    CSHORT Milliseconds;// range [0..999]
    CSHORT Weekday;     // range [0..6] == [Sunday..Saturday]
} TIME_FIELDS;
typedef TIME_FIELDS *PTIME_FIELDS;

LISTENER_INFO ListenerInfo;

HWND EventList::DialogHwnd = NULL;

/////////////////////////////////////////////////////////////////////////////
//	GMTtoLocalTime:	Convert from GMT to local time
//					Can't find an easy function to do this

CTime GMTtoLocalTime( CTime CTgmt)
{
	static boolean NotGotDiff = true;
	static time_t diff = 0;
	time_t gmt = CTgmt.GetTime();
	if( NotGotDiff)
	{
		// Get GMT to Local time difference by looking at current time
		time_t Now = time(NULL);
		struct tm* TM = gmtime(&Now);
		CTime gmtNow(TM->tm_year+1900,TM->tm_mon+1,TM->tm_mday,TM->tm_hour,TM->tm_min,TM->tm_sec);
		diff = gmtNow.GetTime()-Now;
		NotGotDiff = false;
	}
	return CTime(gmt-diff);
}

/////////////////////////////////////////////////////////////////////////////

HANDLE GetDeviceViaInterfaceOl( GUID* pGuid, DWORD instance);

/////////////////////////////////////////////////////////////////////////////
// Copied from WDM.H


EventList::EventList(){m_Len=0;}

void EventList:: InsertList(DebugPrint_Event* pEvent)
{
	for(int i=0;i<m_Len;i++)
	{
		if(strcmp(pEvent->ProcessID,RollEventList[i].ProcessID)==0)
		{
			::SendMessage( DialogHwnd, WM_SEND_MESSAGE, i, (LPARAM)pEvent);
            return;
		}
	}
	strcpy(RollEventList[m_Len].ProcessID,pEvent->ProcessID);
	m_Len=(m_Len+1)%MAXLEN;
	::SendMessage( DialogHwnd, WM_SEND_MESSAGE, m_Len, (LPARAM)pEvent);
}

void EventList::SendEvent(CTime time,
		char *ProcessID,
		char *ProcessName,
		char *Operation,
		char *addr1,
		char *addr2,
		char *addr3,
		char *addr4,
		char *port)
{
	if(DialogHwnd==NULL) return;
	DebugPrint_Event* pEvent = new DebugPrint_Event;
	if( time==0) time = CTime::GetCurrentTime();
	pEvent->Timestamp = time;
	strcpy(pEvent->ProcessID,ProcessID);
	strcpy(pEvent->ProcessName,ProcessName);
	strcpy(pEvent->Operation,Operation);
	strcpy(pEvent->addr1,addr1);
	strcpy(pEvent->addr2,addr2);
	strcpy(pEvent->addr3,addr3);
	strcpy(pEvent->addr4,addr4);
	strcpy(pEvent->port,port);
	
	::SendMessage( DialogHwnd, WM_SEND_MESSAGE, -1, (LPARAM)pEvent);
	
	InsertList(pEvent);
}

UINT ListenThreadFunction( LPVOID pParam)
{
	PLISTENER_INFO pListenerInfo = (PLISTENER_INFO)pParam;
	if (pListenerInfo==NULL) return -1;
    
	EventList *pEvent=new EventList;
	CString StartMsg = "Starting to listen";

	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if( GetVersionEx(&osvi))
	{
		switch( osvi.dwPlatformId)
		{
		case VER_PLATFORM_WIN32_WINDOWS:
			StartMsg.Format("Version %s starting to listen under Windows 9%c (%d.%d build %d) %s",
				Version,
				osvi.dwMinorVersion==0?'5':'8',
				osvi.dwMajorVersion, osvi.dwMinorVersion, LOWORD(osvi.dwBuildNumber), osvi.szCSDVersion);
			break;
		case VER_PLATFORM_WIN32_NT:
			StartMsg.Format("Version %s starting to listen under Windows 2000 (%d.%d build %d) %s",
				Version,
				osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, osvi.szCSDVersion);
			break;
		}
	}
	//pEvent->SendEvent( "Monitor", StartMsg, CTime::GetCurrentTime(), 0, false);

	// Buffer for events
	const int MAX_EVENT_LEN = 1024;
	char Event[MAX_EVENT_LEN+1];

	// Create Overlapped read structure and event
	HANDLE FileIOWaiter = CreateEvent( NULL, TRUE, FALSE, NULL);
	if( FileIOWaiter==NULL)
		goto Exit2;
	OVERLAPPED ol;
	ol.Offset = 0;
	ol.OffsetHigh = 0;
	ol.hEvent = FileIOWaiter;

	// Keep looping, waiting for events, until KeepGoing goes false

	for(;;)
	{
		// Initiate overlapped read
	
		DWORD TxdBytes;
		ResetEvent(FileIOWaiter);
		memset(Event,0,MAX_EVENT_LEN+1);
		if( !ReadFile( ListenerInfo.DebugPrintDriver, Event, MAX_EVENT_LEN, &TxdBytes, &ol))
		{
			// Check for read errors
			if( GetLastError()!=ERROR_IO_PENDING)
			{
				CString Msg;
				Msg.Format("Read didn't return pending %d", GetLastError());
				//EventList::SendEvent( "Monitor", Msg);
				AfxMessageBox("Monitor"+Msg);
				goto Exit;
			}

//			DebugPrint_Event::SendEvent( "Monitor", "Waiting for read completion");

			// Wait for read to complete (check for KeepGoing going false every 100ms)
			while( WaitForSingleObject( FileIOWaiter, 100)==WAIT_TIMEOUT)
			{
				if( !ListenerInfo.KeepGoing)
				{
					// Cancel the pending read
					CancelIo(ListenerInfo.DebugPrintDriver);
					goto Exit;
				}
			}

			// Get read result, ie bytes transferred
			if( !GetOverlappedResult( ListenerInfo.DebugPrintDriver, &ol, &TxdBytes, FALSE))
			{
				CString Msg;
				Msg.Format("GetOverlappedResult failed %d (ol.Internal=%X)",
								GetLastError(),ol.Internal);
				//EventList::SendEvent( "Monitor", Msg);
				AfxMessageBox("Monitor"+Msg);
				continue;
			}
		}

		// Check there's something there
		if( TxdBytes < sizeof(TIME_FIELDS)+2)
		{
			//DebugPrint_Event::SendEvent( "Monitor", "Short read msg" );
			AfxMessageBox( "Monitor: Short read msg" );
			continue;
		}
		// Extract Timestamp, Driver and Msg, and post to View
		Event[MAX_EVENT_LEN] = '\0';

		//时间
		PTIME_FIELDS pTF = (PTIME_FIELDS)Event;
		CTime gmtEventTime(pTF->Year,pTF->Month,pTF->Day,pTF->Hour,pTF->Minute,pTF->Second);
		CTime EventTime = GMTtoLocalTime(gmtEventTime);

		//进程号
		ULONG len=sizeof(TIME_FIELDS);
		char* pProcessID=Event+len;
		len+=strlen(pProcessID)+1;

		//进程名
		char *ProcessName=Event+len;
		len+=strlen(ProcessName)+1;

		//IP地址第一段
		char *addr1=Event+len;
		len+=strlen(addr1)+1;

		//IP地址第二段
		char *addr2=Event+len;
		len+=strlen(addr2)+1;

		//IP地址第三段
		char *addr3=Event+len;
		len+=strlen(addr3)+1;

		//IP地址第四段
		char *addr4=Event+len;
		len+=strlen(addr4)+1;

		//操作
		char *operation=Event+len;
		len+=strlen(operation)+1;

		//端口号
        char* pPort=Event+len;
		
		pEvent->SendEvent(EventTime,
			pProcessID,
			ProcessName,
			operation,
			addr1,
			addr2,
			addr3,
			addr4,
			pPort
			);

	}

Exit:
	CloseHandle(FileIOWaiter);
Exit2:
	CloseHandle(ListenerInfo.DebugPrintDriver);
	ListenerInfo.DebugPrintDriver = NULL;
	//DebugPrint_Event::SendEvent( "Monitor", "Stopped listening");
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//	StartListener:	Called by App to start listener thread

void StartListener()
{
	//EventList::ViewHwnd = pView->GetSafeHwnd();

	// Try to open first DebugPrint driver device
	ListenerInfo.DebugPrintDriver = GetDeviceViaInterfaceOl((LPGUID)&DEBUGPRINT_GUID,0);
	if( ListenerInfo.DebugPrintDriver==NULL)
	{
		//DebugPrint_Event::SendEvent( "Monitor", "Could not open DebugPrint driver" );
		return;
	}
	// Start worker thread
	AfxMessageBox("转发驱动安装成功！");
	ListenerInfo.KeepGoing = true;
	AfxBeginThread(ListenThreadFunction, &ListenerInfo);
}

/////////////////////////////////////////////////////////////////////////////
//	StopListener:	Called by App to tell listener thread to stop

void StopListener()
{
	if( ListenerInfo.DebugPrintDriver==NULL)
		return;

	//DebugPrint_Event::SendEvent( "Monitor", "About to end listen" );
	ListenerInfo.KeepGoing = false;
	AfxMessageBox("转发驱动卸载成功！");
}

/////////////////////////////////////////////////////////////////////////////
//	GetDeviceViaInterfaceOl:	Open DebugPrint device in overlapped mode

HANDLE GetDeviceViaInterfaceOl( GUID* pGuid, DWORD instance)
{
	// Get handle to relevant device information set
	HDEVINFO info = SetupDiGetClassDevs(pGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
	if(info==INVALID_HANDLE_VALUE)
		return NULL;

	// Get interface data for the requested instance
	SP_INTERFACE_DEVICE_DATA ifdata;
	ifdata.cbSize = sizeof(ifdata);
	if(!SetupDiEnumDeviceInterfaces(info, NULL, pGuid, instance, &ifdata))
	{
		SetupDiDestroyDeviceInfoList(info);
		return NULL;
	}

	// Get size of symbolic link name
	DWORD ReqLen;
	SetupDiGetDeviceInterfaceDetail(info, &ifdata, NULL, 0, &ReqLen, NULL);
	PSP_INTERFACE_DEVICE_DETAIL_DATA ifDetail = (PSP_INTERFACE_DEVICE_DETAIL_DATA)(new char[ReqLen]);
	if( ifDetail==NULL)
	{
		SetupDiDestroyDeviceInfoList(info);
		return NULL;
	}

	// Get symbolic link name
	ifDetail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
	if( !SetupDiGetDeviceInterfaceDetail(info, &ifdata, ifDetail, ReqLen, NULL, NULL))
	{
		SetupDiDestroyDeviceInfoList(info);
		delete ifDetail;
		return NULL;
	}

	// Open file
	HANDLE rv = CreateFile( ifDetail->DevicePath, 
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED, NULL);
	if( rv==INVALID_HANDLE_VALUE) rv = NULL;

	// Tidy up
	delete ifDetail;
	SetupDiDestroyDeviceInfoList(info);
	return rv;
}

/////////////////////////////////////////////////////////////////////////////

