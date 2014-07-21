//////////////////////////////////////////////////////////////////////////////
//	Copyright © 1998-1999 Chris Cant, PHD Computer Consultants Ltd
//
//	DebugPrint WDM driver
/////////////////////////////////////////////////////////////////////////////
//	pnp.cpp:		Plug and Play and Power IRP handlers
/////////////////////////////////////////////////////////////////////////////
//	DbpAddDevice					Unload driver routine
//	DbpPnp							PNP IRP dispatcher
//	PnpQueryRemoveDeviceHandler		Handle PnP query remove device
//	PnpSurpriseRemovalHandler		Handle PnP surprise removal
//	PnpRemoveDeviceHandler			Handle PnP remove device
//	PnpStopDeviceHandler			Handle PnP stop device
//*	ClearQueues						Clear read queue and queued events
//*	PnpStopDevice					Handle PnP device stopping
//*	PnpDefaultHandler				Default PnP handler to pass to next stack device
//*	ForwardIrpAndWait				Forward IRP and wait for it to complete
//*	ForwardedIrpCompletionRoutine	Completion routine for ForwardIrpAndWait
//*	LockDevice						Lock out PnP remove request
//*	UnlockDevice					Unlock device allow PnP remove request
//	DbpPower						POWER IRP dispatcher
/////////////////////////////////////////////////////////////////////////////
//	Version history
//	20-Oct-98	1.0.0	CC	creation
//	20-Nov-98	1.01	CC	Better PnP support
//	20-Nov-98	1.01	CC	ClearQueues cancels correct IRP
//	25-Nov-98	1.02	CC	Even better PnP support
//	25-Nov-98	1.02	CC	StoreStartedEvent call added
//	16-Feb-99	1.03	CC	RtlInitUnicodeString used
/////////////////////////////////////////////////////////////////////////////

#define INITGUID // initialize DEBUGPRINT_GUID in this module

#include "DebugPrint.h"

#pragma code_seg("PAGE")	// start PAGE section

/////////////////////////////////////////////////////////////////////////////
//	Local functions

NTSTATUS PnpQueryRemoveDeviceHandler(IN PDEVICE_OBJECT fdo, IN PIRP Irp);
NTSTATUS PnpSurpriseRemovalHandler( IN PDEVICE_OBJECT fdo, IN PIRP Irp);
NTSTATUS PnpRemoveDeviceHandler(IN PDEVICE_OBJECT fdo, IN PIRP Irp);
NTSTATUS PnpStopDeviceHandler(IN PDEVICE_OBJECT fdo, IN PIRP Irp);
NTSTATUS PnpDefaultHandler(IN PDEVICE_OBJECT fdo, IN PIRP Irp);

NTSTATUS ForwardIrpAndWait( IN PDEVICE_OBJECT fdo, IN PIRP Irp);
NTSTATUS ForwardedIrpCompletionRoutine( IN PDEVICE_OBJECT fdo, IN PIRP Irp, IN PKEVENT ev);

void ClearQueues(PDEBUGPRINT_DEVICE_EXTENSION dx);
void PnpStopDevice( IN PDEBUGPRINT_DEVICE_EXTENSION dx);

/////////////////////////////////////////////////////////////////////////////
//	DbpAddDevice:
//
//	Description:
//		Cope with a new Pnp device being added here.
//		Usually just attach to the top of the driver stack.
//		Do not talk to device here!
//
//	Arguments:
//		Pointer to the Driver object
//		Pointer to Physical Device Object
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpAddDevice(	IN PDRIVER_OBJECT DriverObject,
						IN PDEVICE_OBJECT pdo)
{
	NTSTATUS status;
    PDEVICE_OBJECT fdo;

	UNICODE_STRING DebugPrintName;
	RtlInitUnicodeString( &DebugPrintName, L"\\Device\\PHDDebugPrint");

	// Create our Functional Device Object in fdo
	status = IoCreateDevice (DriverObject,
		sizeof(DEBUGPRINT_DEVICE_EXTENSION),
		&DebugPrintName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,	// Not exclusive
		&fdo);
	if( !NT_SUCCESS(status))
		return status;

	// Remember fdo in our device extension
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	dx->fdo = fdo;
	dx->UsageCount = 1;
	KeInitializeEvent( &dx->StoppingEvent, NotificationEvent, FALSE);
	dx->OpenHandleCount = 0;
	dx->GotResources = false;
	dx->Paused = false;
	dx->IODisabled = true;
	dx->Stopping = false;

	// Initialise event list
	KeInitializeSpinLock(&dx->EventListLock);
	InitializeListHead(&dx->EventList);

	// Initialise "read queue"
	KeInitializeSpinLock(&dx->ReadIrpLock);
	dx->ReadIrp = NULL;

	// Register and enable our device interface
	status = IoRegisterDeviceInterface(pdo, &DEBUGPRINT_GUID, NULL, &dx->ifSymLinkName);
	if( !NT_SUCCESS(status))
	{
		IoDeleteDevice(fdo);
		return status;
	}

	// Attach to the driver stack below us
	dx->NextStackDevice = IoAttachDeviceToDeviceStack(fdo,pdo);

	// Set fdo flags appropriately
	fdo->Flags |= DO_BUFFERED_IO|DO_POWER_PAGABLE;
	fdo->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
//	DbpPnp:
//
//	Description:
//		Handle IRP_MJ_PNP requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			Various minor codes
//				IrpStack->Parameters.QueryDeviceRelations
//				IrpStack->Parameters.QueryInterface
//				IrpStack->Parameters.DeviceCapabilities
//				IrpStack->Parameters.FilterResourceRequirements
//				IrpStack->Parameters.ReadWriteConfig
//				IrpStack->Parameters.SetLock
//				IrpStack->Parameters.QueryId
//				IrpStack->Parameters.QueryDeviceText
//				IrpStack->Parameters.UsageNotification
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpPnp(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx=(PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	if (!LockDevice(dx))
		return CompleteIrp(Irp, STATUS_DELETE_PENDING, 0);

	// Get minor function
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG MinorFunction = IrpStack->MinorFunction;

	NTSTATUS status = STATUS_SUCCESS;
	switch( MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		dx->GotResources = true;
		dx->Paused = false;
		dx->IODisabled = false;
		status = PnpDefaultHandler(fdo,Irp);
		IoSetDeviceInterfaceState(&dx->ifSymLinkName, TRUE);

		// Store DebugPrint started event
		StoreStartedEvent(dx);
		break;

	case IRP_MN_QUERY_REMOVE_DEVICE:
		status = PnpQueryRemoveDeviceHandler(fdo,Irp);
		break;
	case IRP_MN_SURPRISE_REMOVAL:
		status = PnpSurpriseRemovalHandler(fdo,Irp);
		break;
	case IRP_MN_REMOVE_DEVICE:
		status = PnpRemoveDeviceHandler(fdo,Irp);
		break;
	case IRP_MN_QUERY_STOP_DEVICE:
		dx->Paused = true;
		dx->IODisabled = true;
		status = PnpDefaultHandler(fdo,Irp);
		break;
	case IRP_MN_STOP_DEVICE:
		status = PnpStopDeviceHandler(fdo,Irp);
		break;
	case IRP_MN_CANCEL_REMOVE_DEVICE:	// fall thru
	case IRP_MN_CANCEL_STOP_DEVICE:
		dx->Paused = false;
		dx->IODisabled = false;
		// fall thru
	default:
		status = PnpDefaultHandler(fdo,Irp);
	}

	UnlockDevice(dx);
	return status;
}

/////////////////////////////////////////////////////////////////////////////
//	PnpQueryRemoveDeviceHandler:	Handle PnP query remove device

NTSTATUS PnpQueryRemoveDeviceHandler(IN PDEVICE_OBJECT fdo, IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx=(PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	if( dx->OpenHandleCount>0)
		return CompleteIrp( Irp, STATUS_UNSUCCESSFUL, 0);
	dx->Paused = true;
	dx->IODisabled = true;
	return PnpDefaultHandler(fdo,Irp);
}

/////////////////////////////////////////////////////////////////////////////
//	PnpSurpriseRemovalHandler:	Handle PnP surprise removal

NTSTATUS PnpSurpriseRemovalHandler( IN PDEVICE_OBJECT fdo, IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx=(PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	// Wait for I/O to complete and stop device
	PnpStopDevice(dx);

	// Pass down stack and carry on immediately
	return PnpDefaultHandler(fdo, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//	PnpRemoveDeviceHandler:	Handle PnP remove device

NTSTATUS PnpRemoveDeviceHandler(IN PDEVICE_OBJECT fdo, IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx=(PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	// Clear read queue and event list
	ClearQueues(dx);

	// Wait for I/O to complete and stop device
	PnpStopDevice(dx);

	// Pass down stack and carry on immediately
	NTSTATUS status = PnpDefaultHandler(fdo, Irp);
	
	// disable device interface
	IoSetDeviceInterfaceState(&dx->ifSymLinkName, FALSE);
	RtlFreeUnicodeString(&dx->ifSymLinkName);
	
	// unattach from stack
	if (dx->NextStackDevice)
		IoDetachDevice(dx->NextStackDevice);

	// delete our fdo
	IoDeleteDevice(fdo);

	return status;
}

/////////////////////////////////////////////////////////////////////////////
//	PnpStopDeviceHandler:	Handle PnP stop device

NTSTATUS PnpStopDeviceHandler( IN PDEVICE_OBJECT fdo, IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx=(PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	// Wait for I/O to complete and stop device
	PnpStopDevice(dx);

	return PnpDefaultHandler( fdo, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//*	ClearQueues:	Clear read queue and queued events

void ClearQueues(PDEBUGPRINT_DEVICE_EXTENSION dx)
{
	// Cancel any queued reads
	PIRP Irp = NULL;
	KIRQL irql;
	KeAcquireSpinLock(&dx->ReadIrpLock,&irql);
	if( dx->ReadIrp!=NULL)
	{
		UnlockDevice(dx);
		Irp = dx->ReadIrp;
		dx->ReadIrp = NULL;
	}
	KeReleaseSpinLock(&dx->ReadIrpLock,irql);
	if( Irp!=NULL)
		CompleteIrp(Irp,STATUS_CANCELLED,0);

	// Clear queued events
	for(;;)
	{
		PLIST_ENTRY pListEntry = ExInterlockedRemoveHeadList( &dx->EventList, &dx->EventListLock);
		if( pListEntry==NULL)
			break;

		PDEBUGPRINT_EVENT pEvent = CONTAINING_RECORD( pListEntry, DEBUGPRINT_EVENT, ListEntry);
		ExFreePool(pEvent);
	}
}

/////////////////////////////////////////////////////////////////////////////
//	PnpStopDevice:	Handle PnP device stopping

void PnpStopDevice( IN PDEBUGPRINT_DEVICE_EXTENSION dx)
{
	// Stop I/O ASAP
	dx->IODisabled = true;

	// Do nothing if we're already stopped
	if( !dx->GotResources)
		return;

	// Wait for any pending I/O operations to complete
	dx->Stopping = true;
	KeResetEvent(&dx->StoppingEvent);
	UnlockDevice(dx);
	UnlockDevice(dx);
	KeWaitForSingleObject( &dx->StoppingEvent, Executive, KernelMode, FALSE, NULL);
	dx->Stopping = false;

	// Bump usage count back up again
	LockDevice(dx);
	LockDevice(dx);
}

/////////////////////////////////////////////////////////////////////////////
//	Support routines
/////////////////////////////////////////////////////////////////////////////
//	PnpDefaultHandler:	Default PnP handler to pass to next stack device

NTSTATUS PnpDefaultHandler( IN PDEVICE_OBJECT fdo, IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx=(PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver( dx->NextStackDevice, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//	ForwardIrpAndWait:	Forward IRP and wait for it to be processed by lower drivers
//						IRP stills needs completing by caller

NTSTATUS ForwardIrpAndWait( IN PDEVICE_OBJECT fdo, IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx=(PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	KEVENT event;
	KeInitializeEvent( &event, NotificationEvent, FALSE);

	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine( Irp, (PIO_COMPLETION_ROUTINE)ForwardedIrpCompletionRoutine,
								(PVOID)&event, TRUE, TRUE, TRUE);

	NTSTATUS status = IoCallDriver( dx->NextStackDevice, Irp);
	if( status==STATUS_PENDING)
	{
		KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL);
		status = Irp->IoStatus.Status;
	}
	return status;
}

/////////////////////////////////////////////////////////////////////////////
//	ForwardedIrpCompletionRoutine:	Completion routine for ForwardIrpAndWait
//	Can be called at DISPATCH_LEVEL IRQL

NTSTATUS ForwardedIrpCompletionRoutine( IN PDEVICE_OBJECT fdo, IN PIRP Irp, IN PKEVENT ev)
{
	KeSetEvent(ev, 0, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

/////////////////////////////////////////////////////////////////////////////
//	LockDevice:	Lock out PnP remove request

bool LockDevice( IN PDEBUGPRINT_DEVICE_EXTENSION dx)
{
	InterlockedIncrement(&dx->UsageCount);

	if( dx->Stopping)
	{
		if( InterlockedDecrement(&dx->UsageCount)==0)
			KeSetEvent( &dx->StoppingEvent, 0, FALSE);
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
//	UnlockDevice:	Unlock device allow PnP remove request

void UnlockDevice( IN PDEBUGPRINT_DEVICE_EXTENSION dx)
{
	LONG UsageCount = InterlockedDecrement(&dx->UsageCount);
	if( UsageCount==0)
		KeSetEvent( &dx->StoppingEvent, 0, FALSE);
}

/////////////////////////////////////////////////////////////////////////////
//	DbpPower:
//
//	Description:
//		Handle IRP_MJ_POWER requests
//
//	Arguments:
//		Pointer to the FDO
//		Pointer to the IRP
//			IRP_MN_WAIT_WAKE:		IrpStack->Parameters.WaitWake.Xxx
//			IRP_MN_POWER_SEQUENCE:	IrpStack->Parameters.PowerSequence.Xxx
//			IRP_MN_SET_POWER:
//			IRP_MN_QUERY_POWER:		IrpStack->Parameters.Power.Xxx
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS DbpPower(	IN PDEVICE_OBJECT fdo,
					IN PIRP Irp)
{
	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	// Just pass to lower driver
	PoStartNextPowerIrp( Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver( dx->NextStackDevice, Irp);
}

/////////////////////////////////////////////////////////////////////////////
#pragma code_seg()	// end PAGE section
