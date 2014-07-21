/////////////////////////////////////////////////////////////////////////////
//	Message id for sending event messages to view

const UINT WM_DEBUGPRINTEVENT = (WM_USER+1);

/////////////////////////////////////////////////////////////////////////////
//	DebugPrint_Event class encapsulates events sent from Listener to View

struct DebugPrint_Event
{
	CTime Timestamp;
	char ProcessID[10];
	char ProcessName[20];
	char Operation[30];
	char addr1[5];
	char addr2[5];
	char addr3[5];
	char addr4[5];
    char RemotePort[8];
	char LocalPort[8];
	char SuccOrFail[15];
};

#define MAXLEN 100					

	   
class EventList
{
public:
	DebugPrint_Event RollEventList[MAXLEN];
	int m_Len;
	static HWND DialogHwnd;
	// Generate and send an event
public:
	EventList();

	void InsertList(DebugPrint_Event* pEvent);
	
	void SendEvent(CTime time,
		char *ProcessID,
		char *ProcessName,
		char *Operation,
		char *addr1,
		char *addr2,
		char *addr3,
		char *addr4,
		char *RemotePort,
		char *LocalPort,
		char *result);
	
};

/////////////////////////////////////////////////////////////////////////////
