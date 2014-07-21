//////////////////////////////////////////////////////////////////////////////
//	Copyright © 1998-1999 Chris Cant, PHD Computer Consultants Ltd
//
//	DebugPrint WDM driver
/////////////////////////////////////////////////////////////////////////////
//	DebugPrint.h			Common header
/////////////////////////////////////////////////////////////////////////////
//	Version history
//	20-Oct-98	1.0.0	CC	creation
//	20-Nov-98	1.01	CC	Better PnP support
//	25-Nov-98	1.02	CC	Even better PnP support
/////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////
//	GUID header

#include "..\DbgPrtGUID.h"

/////////////////////////////////////////////////////////////////////////////
//	Our device extension

typedef struct _DEBUGPRINT_DEVICE_EXTENSION
{
	PDEVICE_OBJECT	fdo;
	PDEVICE_OBJECT	NextStackDevice;
	UNICODE_STRING	ifSymLinkName;

	bool GotResources;	// Not stopped
	bool Paused;		// Stop or remove pending
	bool IODisabled;	// Paused or stopped

	LONG OpenHandleCount;	// Count of open handles

	LONG UsageCount;		// Pending I/O Count
	bool Stopping;			// In process of stopping
	KEVENT StoppingEvent;	// Set when all pending I/O complete

	PIRP ReadIrp;				// "Read queue" of 1 IRP
	KSPIN_LOCK ReadIrpLock;		// Spin lock to guard access to ReadIrp

	LIST_ENTRY EventList;		// Doubly-linked list of written Events
	KSPIN_LOCK EventListLock;	// Spin lock to guard access to EventList

} DEBUGPRINT_DEVICE_EXTENSION, *PDEBUGPRINT_DEVICE_EXTENSION;

/////////////////////////////////////////////////////////////////////////////
//	DebugPrint Event structure (put in doubly-linked EventList in dev ext)

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

/////////////////////////////////////////////////////////////////////////////
// Forward declarations of functions

VOID DbpUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS DbpPower(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp);

NTSTATUS DbpPnp(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp);

NTSTATUS DbpAddDevice(	IN PDRIVER_OBJECT DriverObject,
						IN PDEVICE_OBJECT pdo);

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

NTSTATUS DbpSystemControl(	IN PDEVICE_OBJECT fdo,
							IN PIRP Irp);

/////////////////////////////////////////////////////////////////////////////

void StoreStartedEvent( IN PDEBUGPRINT_DEVICE_EXTENSION dx);

bool LockDevice( IN PDEBUGPRINT_DEVICE_EXTENSION dx);
void UnlockDevice( IN PDEBUGPRINT_DEVICE_EXTENSION dx);

NTSTATUS CompleteIrp( PIRP Irp, NTSTATUS status, ULONG info);

/////////////////////////////////////////////////////////////////////////////
