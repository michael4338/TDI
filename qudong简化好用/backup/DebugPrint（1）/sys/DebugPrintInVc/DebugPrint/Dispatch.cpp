//////////////////////////////////////////////////////////////////////////////
//	Copyright © 1998 Chris Cant, PHD Computer Consultants Ltd
//
//	DebugPrint WDM driver
/////////////////////////////////////////////////////////////////////////////
//	dispatch.cpp:		I/O IRP handlers
/////////////////////////////////////////////////////////////////////////////
//	DbpCreate			Handle Create/Open file IRP
//	DbpClose			Handle Close file IRPs
//	DbpCancelIrp		Cancel queued read IRP
//	ReadEvent			Read an event from EventList
//	DbpRead				Handle Read IRPs
//	DbpWrite			Handle Write IRPs
//*	StoreStartedEvent	Store a DebugPrint device started event
//*	ANSIstrlen			Get ANSI string length
//	DbpDeviceControl	Handle DeviceIoControl IRPs
//	DbpSystemControl	Handle WMI IRPs
/////////////////////////////////////////////////////////////////////////////
//	Version history
//	20-Oct-98	1.0.0	CC	creation
//	20-Nov-98	1.01	CC	Better PnP support
//	25-Nov-98	1.02	CC	Even better PnP support
//	25-Nov-98	1.02	CC	StoreStartedEvent added
//	26-Apr-99	1.03	CC	Cancel routine cleared in ReadEvent
/////////////////////////////////////////////////////////////////////////////

#include "DebugPrint.h"

#pragma code_seg("PAGE")	// start PAGE section

/////////////////////////////////////////////////////////////////////////////

USHORT ANSIstrlen( char* str);

/////////////////////////////////////////////////////////////////////////////
//	DbpCreate:
//
//	Description:
//		Handle IRP_MJ_CREATE requests
//		Don't bother to lock device
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->Parameters.Create.xxx has create parameters
//			IrpStack->FileObject->FileName has file name on device
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpCreate(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	if( dx->IODisabled)
		return CompleteIrp( Irp, STATUS_DEVICE_NOT_CONNECTED, 0);

	InterlockedIncrement(&dx->OpenHandleCount);

	// Complete successfully
	return CompleteIrp(Irp,STATUS_SUCCESS,0);
}

/////////////////////////////////////////////////////////////////////////////
//	DbpClose:
//
//	Description:
//		Handle IRP_MJ_CLOSE requests
//		Allow closes to complete if device not started
//		Don't bother to lock device
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpClose(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	InterlockedDecrement(&dx->OpenHandleCount);

	// Complete successfully
	return CompleteIrp(Irp,STATUS_SUCCESS,0);
}
 
/////////////////////////////////////////////////////////////////////////////
//	DbpCancelIrp:
//
//	Description:
//		Cancel this IRP.
//			Called to cancel a Irp.
//			Called when CancelIo called or process finished without closing handle.
//			IRP must have set this as its cancel routine.
//		Only one read queue IRP has its cancel routine set in DebugPrint.
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP

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
		UnlockDevice(dx);
		dx->ReadIrp = NULL;
	}
	KeReleaseSpinLock(&dx->ReadIrpLock,irql);

	// Whatever Irp it is, just cancel it
	CompleteIrp(Irp,STATUS_CANCELLED,0);
}

/////////////////////////////////////////////////////////////////////////////
//	ReadEvent:
//
//	Description:
//		Sees if the EventList has a pending event
//		If so, copies data into given Irp and completes it
//		Event memory is deleted
//
//		Usually holding ReadIrpLock
//
//	Arguments:
//		Pointer to our dx
//		Pointer to the IRP
//
//	Return Value:
//		true if event was read, ie Irp completed

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

/////////////////////////////////////////////////////////////////////////////
//	DbpRead:
//
//	Description:
//		Handle IRP_MJ_READ requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->Parameters.Read.xxx has read parameters
//			User buffer at:	AssociatedIrp.SystemBuffer	(buffered I/O)
//							MdlAddress					(direct I/O)
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpRead(IN PDEVICE_OBJECT fdo,
				IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	if( dx->IODisabled)
		return CompleteIrp( Irp, STATUS_DEVICE_NOT_CONNECTED, 0);
	if( !LockDevice(dx))
		return CompleteIrp( Irp, STATUS_DELETE_PENDING, 0);

	// Only one listening read allowed at a time.
	KIRQL irql;
	KeAcquireSpinLock(&dx->ReadIrpLock,&irql);
	if( dx->ReadIrp!=NULL)
	{
		KeReleaseSpinLock(&dx->ReadIrpLock,irql);
		UnlockDevice(dx);
		return CompleteIrp(Irp,STATUS_UNSUCCESSFUL,0);;
	}

	// See if there's data available
	if( ReadEvent( dx, Irp))
	{
		KeReleaseSpinLock(&dx->ReadIrpLock,irql);
		UnlockDevice(dx);
		return STATUS_SUCCESS;
	}

	// No event is available, queue this read Irp
	dx->ReadIrp = Irp;
	KeReleaseSpinLock(&dx->ReadIrpLock,irql);

	// Mark Irp as pending and set Cancel routine
	Irp->IoStatus.Information = 0;
	IoMarkIrpPending(Irp);
	IoSetCancelRoutine(Irp,DbpCancelIrp);

	return STATUS_PENDING;
}

/////////////////////////////////////////////////////////////////////////////
//	DbpWrite:
//
//	Description:
//		Handle IRP_MJ_WRITE requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->Parameters.Write.xxx has write parameters
//			User buffer at:	AssociatedIrp.SystemBuffer	(buffered I/O)
//							MdlAddress					(direct I/O)
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpWrite(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	if( dx->IODisabled)
		return CompleteIrp( Irp, STATUS_DEVICE_NOT_CONNECTED, 0);
	if( !LockDevice(dx))
		return CompleteIrp( Irp, STATUS_DELETE_PENDING, 0);

	PIO_STACK_LOCATION	IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG BytesTxd = 0;

	// Check write len
	ULONG WriteLen = IrpStack->Parameters.Write.Length;
	if( WriteLen==0)
	{
		UnlockDevice(dx);
		return CompleteIrp(Irp,STATUS_SUCCESS,0);
	}

	// Copy write data into an event
	ULONG Len = sizeof(LIST_ENTRY)+sizeof(ULONG)+WriteLen;
	PDEBUGPRINT_EVENT pEvent = (PDEBUGPRINT_EVENT)ExAllocatePool(NonPagedPool,Len);
	if( pEvent==NULL)
	{
		UnlockDevice(dx);
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
			UnlockDevice(dx);
			dx->ReadIrp = NULL;
		}
	KeReleaseSpinLock(&dx->ReadIrpLock,irql);

	// Complete IRP
	UnlockDevice(dx);
	return CompleteIrp(Irp,STATUS_SUCCESS,WriteLen);
}

/////////////////////////////////////////////////////////////////////////////
//*	StoreStartedEvent:	Store a DebugPrint device started event

void StoreStartedEvent( IN PDEBUGPRINT_DEVICE_EXTENSION dx)
{
	// Get current time

}

//////////////////////////////////////////////////////////////////////////////
//*	ANSIstrlen:	Return length of null terminated ANSI string

USHORT ANSIstrlen( char* str)
{
	USHORT len = 0;
	for(;*str++!='\0';)
		len++;
	return len;
}

/////////////////////////////////////////////////////////////////////////////
//	DbpDeviceControl:
//
//	Description:
//		Handle IRP_MJ_DEVICE_CONTROL requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			Buffered:	AssociatedIrp.SystemBuffer (and IrpStack->Parameters.DeviceIoControl.Type3InputBuffer)
//			Direct:		MdlAddress
//
//			IrpStack->Parameters.DeviceIoControl.InputBufferLength
//			IrpStack->Parameters.DeviceIoControl.OutputBufferLength 
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpDeviceControl(	IN PDEVICE_OBJECT fdo,
							IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	if( dx->IODisabled)
		return CompleteIrp( Irp, STATUS_DEVICE_NOT_CONNECTED, 0);

	// Complete successfully
	return CompleteIrp(Irp,STATUS_SUCCESS,0);
}

/////////////////////////////////////////////////////////////////////////////
//	DbpSystemControl:
//
//	Description:
//		Handle IRP_MJ_SYSTEM_CONTROL requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			Various minor parameters
//			IrpStack->Parameters.WMI.xxx has WMI parameters
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpSystemControl(	IN PDEVICE_OBJECT fdo,
							IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	if( dx->IODisabled)
		return CompleteIrp( Irp, STATUS_DEVICE_NOT_CONNECTED, 0);

	// Just pass to lower driver
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver( dx->NextStackDevice, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//	DbpCleanup:
//
//	Description:
//		Handle IRP_MJ_CLEANUP requests
//		Cancel queued IRPs which match given FileObject
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->FileObject has handle to file
//
//	Return Value:
//		This function returns STATUS_XXX

//	Not needed for DebugPrint

/////////////////////////////////////////////////////////////////////////////

NTSTATUS CompleteIrp( PIRP Irp, NTSTATUS status, ULONG info)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp,IO_NO_INCREMENT);
	return status;
}

/////////////////////////////////////////////////////////////////////////////
#pragma code_seg()	// end PAGE section
