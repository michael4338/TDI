
#include "DebugPrint.h"


//全局变量	
ULONG           ulDeviceNumber;             	
KSPIN_LOCK		DeviceNumberSpinLock;


#pragma code_seg("INIT") // start PAGE section

extern "C"
NTSTATUS DriverEntry(	IN PDRIVER_OBJECT DriverObject,
						IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_OBJECT fdo;

	KeInitializeSpinLock(&DeviceNumberSpinLock);
	KIRQL   OldIrql;                                                                     
    KeAcquireSpinLock(&DeviceNumberSpinLock, &OldIrql);                                                   
    ulDeviceNumber=(unsigned long)1;   
    KeReleaseSpinLock(&DeviceNumberSpinLock, OldIrql);

    UNICODE_STRING wszTem1;
    RtlInitUnicodeString(&wszTem1,NT_DEVICE_NAME);
    UNICODE_STRING wszTem2;
    wszTem2.Length =0;
    wszTem2.MaximumLength = 32;
    wszTem2.Buffer=(PWSTR)ExAllocatePoolWithTag(PagedPool, wszTem2.MaximumLength , 1633);

    status=RtlIntegerToUnicodeString(ulDeviceNumber,0,&wszTem2);

    if (!NT_SUCCESS(status))
		return status;

    UNICODE_STRING ntDeviceName;
    ntDeviceName.Length =wszTem1.Length + wszTem2.Length ;

    ntDeviceName.MaximumLength =ntDeviceName.Length +2;
    ntDeviceName.Buffer =(PWSTR)ExAllocatePoolWithTag(PagedPool, ntDeviceName.MaximumLength , 1633);
    RtlCopyUnicodeString(&ntDeviceName, (PUNICODE_STRING)&wszTem1);
    RtlAppendUnicodeStringToString(&ntDeviceName, (PUNICODE_STRING)&wszTem2);
    ntDeviceName.Buffer[ ntDeviceName.Length /2] = UNICODE_NULL;
	
	status = IoCreateDevice (DriverObject,
		sizeof(DEBUGPRINT_DEVICE_EXTENSION),
		&(UNICODE_STRING)ntDeviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&fdo);
	if( !NT_SUCCESS(status))
	{
		DbgPrint("CREATE DEBUGPRINT FAIL");
		ExFreePool( (PVOID)(wszTem2.Buffer ));
		ExFreePool( (PVOID)(ntDeviceName.Buffer ));
		return status;
	}
	
	DbgPrint("CREATE OK!");
	ExFreePool((PVOID)(wszTem2.Buffer));
                                                                     
    KeAcquireSpinLock(&DeviceNumberSpinLock, &OldIrql);                                                   
    ulDeviceNumber+=(unsigned long)1;   
    KeReleaseSpinLock(&DeviceNumberSpinLock, OldIrql);

	UNICODE_STRING wszTem3;
    RtlInitUnicodeString(&wszTem3,DOS_DEVICE_NAME);
    UNICODE_STRING wszTem4;
    wszTem4.Length =0;
    wszTem4.MaximumLength = 32;
    wszTem4.Buffer=(PWSTR)ExAllocatePoolWithTag(PagedPool, wszTem4.MaximumLength , 1633);

    status=RtlIntegerToUnicodeString(ulDeviceNumber,0,&wszTem4);
    if (!NT_SUCCESS(status))
		return status;

    UNICODE_STRING win32DeviceName;
    win32DeviceName.Length =wszTem3.Length +wszTem4.Length ;
    win32DeviceName.MaximumLength =win32DeviceName.Length +2;
    win32DeviceName.Buffer =(PWSTR)ExAllocatePoolWithTag(PagedPool, win32DeviceName.MaximumLength , 1633);
    RtlCopyUnicodeString(&win32DeviceName, (PUNICODE_STRING)&wszTem3);
    RtlAppendUnicodeStringToString(&win32DeviceName, (PUNICODE_STRING)&wszTem4);
    win32DeviceName.Buffer[ win32DeviceName.Length /2] = UNICODE_NULL;

	status = IoCreateSymbolicLink( &(UNICODE_STRING)win32DeviceName,
		&(UNICODE_STRING)ntDeviceName);
    if (!NT_SUCCESS(status)) 
	{ 
        DbgPrint( "TWDM: IoCreateSymbolicLink() faild ! n" );
		ExFreePool( (PVOID)(wszTem4.Buffer ));
		ExFreePool( (PVOID)(win32DeviceName.Buffer ));
        ExFreePool( (PVOID)(ntDeviceName.Buffer ));
		IoDeleteDevice(fdo);
	} 
    ExFreePool( (PVOID)(wszTem4.Buffer ));
    ExFreePool( (PVOID)(ntDeviceName.Buffer ));

	DbgPrint( "TWDM: IoCreateSymbolicLink() ok ! n" ); 

	PDEBUGPRINT_DEVICE_EXTENSION dx = (PDEBUGPRINT_DEVICE_EXTENSION)fdo->DeviceExtension;

	//初始化
	dx->ifSymLinkName = TRUE; 
    dx->ustrSymLinkName=win32DeviceName;
	dx->fdo = fdo;
	dx->Banned   = true;
	KeInitializeSpinLock(&dx->EventListLock);
	InitializeListHead(&dx->EventList);

	KeInitializeSpinLock(&dx->ReadIrpLock);
	dx->ReadIrp = NULL;

	fdo->Flags |= DO_BUFFERED_IO|DO_POWER_PAGABLE;
	fdo->Flags &= ~DO_DEVICE_INITIALIZING;

 	DriverObject->DriverUnload = DbpUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DbpCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DbpClose;

	DriverObject->MajorFunction[IRP_MJ_READ] = DbpRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = DbpWrite;

	return status;
}
#pragma code_seg() // end INIT section


#pragma code_seg("PAGE") // start PAGE section

VOID DbpUnload(IN PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT               pDeviceObject;
    PDEVICE_OBJECT               pNextDeviceObject;
    PDEBUGPRINT_DEVICE_EXTENSION dx;

    pDeviceObject = DriverObject->DeviceObject;

    while( pDeviceObject != NULL )
    {
        pNextDeviceObject = pDeviceObject->NextDevice;
        dx = (PDEBUGPRINT_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;
        if(dx->ifSymLinkName)
		{
            ExFreePool((PVOID)(dx->ustrSymLinkName.Buffer));
		}
		ClearQueues(dx);
		IoDeleteDevice(dx->fdo);
        pDeviceObject = pNextDeviceObject;
	}
}
void ClearQueues(PDEBUGPRINT_DEVICE_EXTENSION dx)
{
	// Cancel any queued reads
	PIRP Irp = NULL;
	KIRQL irql;
	KeAcquireSpinLock(&dx->ReadIrpLock,&irql);
	if( dx->ReadIrp!=NULL)
	{
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

//////////////////////////////////////////////////////////////////////////////
#pragma code_seg() // end PAGE section
