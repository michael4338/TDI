
#define VERSION_STARTED_MESSAGE "Version 1.03 started"

/////////////////////////////////////////////////////////////////////////////
//	Include WDM standard header with C linkage

#ifdef __cplusplus
extern "C"
{
#endif

#include "wdm.h"

#ifdef __cplusplus
}
#endif

#define NT_DEVICE_NAME L"\\Device\\PHDDebugPrint" 
#define DOS_DEVICE_NAME L"\\DosDevices\\PHDDebugPrint"

/////////////////////////////////////////////////////////////////////////////
//	Our device extension

typedef struct _DEBUGPRINT_DEVICE_EXTENSION
{
	PDEVICE_OBJECT	fdo;

	bool	        ifSymLinkName;
    UNICODE_STRING  ustrSymLinkName;

	PIRP ReadIrp;				// "Read queue" of 1 IRP
	KSPIN_LOCK ReadIrpLock;		// Spin lock to guard access to ReadIrp

	LIST_ENTRY EventList;		// Doubly-linked list of written Events
	KSPIN_LOCK EventListLock;	// Spin lock to guard access to EventList

	bool Banned;

} DEBUGPRINT_DEVICE_EXTENSION, *PDEBUGPRINT_DEVICE_EXTENSION;


typedef struct _DEBUGPRINT_EVENT
{
	LIST_ENTRY ListEntry;
	ULONG Len;
	UCHAR EventData[1];
} DEBUGPRINT_EVENT, *PDEBUGPRINT_EVENT;

typedef struct _EVENT
{
	ULONG ProcessID;
	char *ProcessName;
    char *Operation;
	char *addr1;
	char *addr2;
	char *addr3;
	char *addr4;
	ULONG port;
}EVENT,*PEVENT;

VOID DbpUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS DbpCreate(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp);

NTSTATUS DbpClose(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp);
 
NTSTATUS DbpWrite(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp);

NTSTATUS DbpRead(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp);

NTSTATUS DbpDeviceControl(	IN PDEVICE_OBJECT fdo,
							IN PIRP Irp);

NTSTATUS CompleteIrp( PIRP Irp, NTSTATUS status, ULONG info);

void ClearQueues(PDEBUGPRINT_DEVICE_EXTENSION dx);
/////////////////////////////////////////////////////////////////////////////
