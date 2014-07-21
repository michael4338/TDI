
#include "DebugPrint.h"

#pragma code_seg("PAGE")	// start PAGE section

/////////////////////////////////////////////////////////////////////////////

USHORT ANSIstrlen( char* str);

NTSTATUS DbpCreate(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	DbgPrint("The Device is On Work");
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	
	return CompleteIrp(Irp,STATUS_SUCCESS,0);
}


NTSTATUS DbpClose(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	// Complete successfully
	return CompleteIrp(Irp,STATUS_SUCCESS,0);
}
 

VOID DbpCancelIrp(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	IoReleaseCancelSpinLock(Irp->CancelIrql);

	// If this is our queued read, then unqueue it
	KIRQL irql;
	KeAcquireSpinLock(&dx->ReadIrpLock,&irql);
	if( Irp==dx->ReadIrp)
	{
		dx->ReadIrp = NULL;
	}
	KeReleaseSpinLock(&dx->ReadIrpLock,irql);

	// Whatever Irp it is, just cancel it
	CompleteIrp(Irp,STATUS_CANCELLED,0);
}


bool ReadEvent( PDEBUGPRINT_DEVICE_EXTENSION dx, PIRP Irp)
{
	// Try to remove Event from EventList
	PLIST_ENTRY pListEntry = ExInterlockedRemoveHeadList( &dx->EventList, &dx->EventListLock);
	if( pListEntry==NULL)
		return false;

	// Get event as DEBUGPRINT_EVENT
	PDEBUGPRINT_EVENT pEvent = CONTAINING_RECORD( pListEntry, DEBUGPRINT_EVENT, ListEntry);

	// Get length of event data
	ULONG EventDataLen = pEvent->Len;

	// Get max read length acceptible
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG ReadLen = IrpStack->Parameters.Read.Length;

	// Shorten event length if necessary
	if( EventDataLen>ReadLen)
		EventDataLen = ReadLen;

	// Copy data to Irp and complete it
	RtlCopyMemory( Irp->AssociatedIrp.SystemBuffer, pEvent->EventData, EventDataLen);
	IoSetCancelRoutine(Irp,NULL);
	CompleteIrp(Irp,STATUS_SUCCESS,EventDataLen);

	// Free event memory
	ExFreePool(pEvent);
	return true;
}


NTSTATUS DbpRead(IN PDEVICE_OBJECT fdo,
				IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	
	// Only one listening read allowed at a time.

	KIRQL irql;
	KeAcquireSpinLock(&dx->ReadIrpLock,&irql);
	if( dx->ReadIrp!=NULL)
	{
		DbgPrint("STATUS_UNSUCCESSFUL!!!");
		KeReleaseSpinLock(&dx->ReadIrpLock,irql);
		return CompleteIrp(Irp,STATUS_UNSUCCESSFUL,0);
	}

	// See if there's data available
	if( ReadEvent( dx, Irp))
	{
		KeReleaseSpinLock(&dx->ReadIrpLock,irql);
		return STATUS_SUCCESS;
	}

	// No event is available, queue this read Irp
	dx->ReadIrp = Irp;
	KeReleaseSpinLock(&dx->ReadIrpLock,irql);

	// Mark Irp as pending and set Cancel routine
	Irp->IoStatus.Information = 0;
	IoMarkIrpPending(Irp);
	IoSetCancelRoutine(Irp,DbpCancelIrp);

	DbgPrint("STATUS_PENDING!!!");
	return STATUS_PENDING;
}


NTSTATUS DbpWrite(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	PIO_STACK_LOCATION	IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG BytesTxd = 0;

	// Check write len
	ULONG WriteLen = IrpStack->Parameters.Write.Length;
	if( WriteLen==0)
	{
		return CompleteIrp(Irp,STATUS_SUCCESS,0);
	}

	if(WriteLen<=3)
	{
		char* EventWrite=(char*)ExAllocatePool(NonPagedPool,WriteLen);
		if(EventWrite==NULL)
		{
			return CompleteIrp(Irp,STATUS_INSUFFICIENT_RESOURCES,0);
		}

		RtlCopyMemory( EventWrite, Irp->AssociatedIrp.SystemBuffer, WriteLen);
	    DbgPrint("EVENTWRITE's first charactor is:%d",EventWrite[0]);

		if(EventWrite[0]=='T')
		{		
			DbgPrint("THE WRITING IS TERMINATED");		
			dx->Banned=true;
		}
		else if(EventWrite[0]=='S')
		{
			DbgPrint("THE WRITING IS RESTARTED");
			dx->Banned=false;
		}
		return CompleteIrp(Irp,STATUS_SUCCESS,0);
	}

	if(dx->Banned)
	{
		DbgPrint("The DebugPrint Driver Is Not Ready Yet");
		return CompleteIrp( Irp, STATUS_DEVICE_NOT_CONNECTED, 0);
	}

	// Copy write data into an event
	ULONG Len = sizeof(LIST_ENTRY)+sizeof(ULONG)+WriteLen;
	PDEBUGPRINT_EVENT pEvent = (PDEBUGPRINT_EVENT)ExAllocatePool(NonPagedPool,Len);
	if( pEvent==NULL)
	{
		return CompleteIrp(Irp,STATUS_INSUFFICIENT_RESOURCES,0);
	}

	pEvent->Len = WriteLen;
	RtlCopyMemory( pEvent->EventData, Irp->AssociatedIrp.SystemBuffer, WriteLen);

	// Insert event into event list
	ExInterlockedInsertTailList(&dx->EventList,&pEvent->ListEntry,&dx->EventListLock);

	DbgPrint("The Writing Operation Has Been Executed");

	// If read pending, then read it
	KIRQL irql;
	KeAcquireSpinLock(&dx->ReadIrpLock,&irql);
	if( dx->ReadIrp!=NULL)
		if( ReadEvent( dx, dx->ReadIrp))
		{
			dx->ReadIrp = NULL;
		}
	KeReleaseSpinLock(&dx->ReadIrpLock,irql);

	return CompleteIrp(Irp,STATUS_SUCCESS,WriteLen);
}

USHORT ANSIstrlen( char* str)
{
	USHORT len = 0;
	for(;*str++!='\0';)
		len++;
	return len;
}


NTSTATUS DbpDeviceControl(	IN PDEVICE_OBJECT fdo,
							IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	return CompleteIrp(Irp,STATUS_SUCCESS,0);
}



NTSTATUS CompleteIrp( PIRP Irp, NTSTATUS status, ULONG info)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return status;
}

/////////////////////////////////////////////////////////////////////////////
#pragma code_seg()	// end PAGE section
