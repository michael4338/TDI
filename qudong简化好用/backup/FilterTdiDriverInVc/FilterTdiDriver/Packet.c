
#include <ndis.h>
#include <tdikrnl.h>
#include <ntddk.h>
#include <stdlib.h>
#include <stdio.h>
#include "packet.h"

static 	ULONG  ProcessNameOffset=0;

NTSTATUS
DriverEntry(
	IN	PDRIVER_OBJECT		DriverObject,
	IN	PUNICODE_STRING		RegistryPath
)
{
	NTSTATUS	status	= 0;
    ULONG		i;
	PEPROCESS curproc;

	curproc = PsGetCurrentProcess();   
    for( i = 0; i < 3*PAGE_SIZE; i++ )
	{
        if( !strncmp( "System", (PCHAR) curproc + i, strlen("System") ))
		{
            ProcessNameOffset = i;
	        break;
		}
	}

	DBGPRINT("DriverEntry Loading...\n");
	DriverObject->DriverUnload = PacketUnload;

	status = TCPFilter_Attach(DriverObject,RegistryPath);

	//加载信息输出模块
	DebugPrintInit("FilterTdiDriver");

    for (i=0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
		DriverObject->MajorFunction[i] = PacketDispatch;
    }

	return status;
}

VOID 
PacketUnload(
	IN PDRIVER_OBJECT		DriverObject
)
{
    PDEVICE_OBJECT			DeviceObject;
    PDEVICE_OBJECT			OldDeviceObject;
	PTDIH_DeviceExtension	pTDIH_DeviceExtension;	

 	DBGPRINT("DriverEntry unLoading...\n");

	DeviceObject = DriverObject->DeviceObject;

	//卸载信息输出模块
	DebugPrintClose();

   while (DeviceObject != NULL) 
	{
        OldDeviceObject = DeviceObject;
		pTDIH_DeviceExtension	
			= (PTDIH_DeviceExtension )DeviceObject->DeviceExtension;
		if( pTDIH_DeviceExtension->NodeType 
			== TDIH_NODE_TYPE_TCP_FILTER_DEVICE )
			TCPFilter_Detach( DeviceObject );   // Calls IoDeleteDevice
		else
			IoDeleteDevice(OldDeviceObject);
        DeviceObject = DeviceObject->NextDevice;
    }
}

NTSTATUS
PacketDispatch(
    IN PDEVICE_OBJECT		DeviceObject,
    IN PIRP					Irp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PTDIH_DeviceExtension	pTDIH_DeviceExtension;
	PIO_STACK_LOCATION		IrpStack;
	PIO_STACK_LOCATION		NextIrpStack;
	PTDI_REQUEST_KERNEL pTDIRequestKernel;
	PTDI_ADDRESS_IP pIPAddress;
	PTRANSPORT_ADDRESS pTransAddr;
	PTDI_CONNECTION_INFORMATION pConn;
	PUCHAR pByte;
	EVENT event;

	int addr1,addr2,addr3,addr4;

	ULONG ProcessId;
	CHAR       ProcessName[16];
	PEPROCESS   curproc;
    int         i;
	char        *nameptr;

	pTDIH_DeviceExtension	
		= (PTDIH_DeviceExtension )(DeviceObject->DeviceExtension);

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

	//得到进程号和进程名
    ProcessId=(ULONG)(PsGetCurrentProcessId());
    //DbgPrint("ProcessId: %d",ProcessId);
	
	if( ProcessNameOffset )
	{
		curproc = PsGetCurrentProcess();
		nameptr = (PCHAR) curproc + ProcessNameOffset;
		strncpy( ProcessName, nameptr, 16 );
		ProcessName[16] = 0;
	}
	else
	{
		strcpy( ProcessName, "???" );
	}

	switch(IrpStack->MajorFunction)
	{
	case IRP_MJ_CREATE:
	 	DBGPRINT("PacketDispatch(IRP_MJ_CREATE)...\n");
		break;
	case IRP_MJ_CLOSE:
	 	//DBGPRINT("PacketDispatch(IRP_MJ_CLOSE)...\n");
		break;
	case IRP_MJ_CLEANUP:
	 	//DBGPRINT("PacketDispatch(IRP_MJ_CLEANUP)...\n");
		break;
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		switch (IrpStack->MinorFunction) 
		{
		case TDI_ACCEPT:
			DBGPRINT("TDI_ACCEPT");
			pConn=(PTDI_CONNECTION_INFORMATION )&IrpStack->Parameters;
		 	pTransAddr=pConn->RemoteAddress;
			if(pTransAddr->Address[0 ].AddressType==TDI_ADDRESS_TYPE_IP)
			{
				pIPAddress = (PTDI_ADDRESS_IP )(PUCHAR )&pTransAddr->Address[0 ].Address;
                pByte = (PUCHAR)&pIPAddress->in_addr;
                DbgPrint( "in_addrXYZACCEPT: %d.%d.%d.%d",pByte[ 0 ], pByte[ 1 ], pByte[ 2 ], pByte[ 3 ] );
				DbgPrint( "sin_portXYZACCEPT: %d ",pIPAddress->sin_port);
			}
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_ACCEPT])...\n");
			break;
		case TDI_ACTION:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_ACTION])...\n");
			break;
		case TDI_ASSOCIATE_ADDRESS:
			DBGPRINT("TDI_ASSOCIATE_ADDRESS");
			//pTDIRequestKernel=(PTDI_REQUEST_KERNEL )&IrpStack->Parameters;
		 	//pTransAddr=(pTDIRequestKernel->RequestConnectionInformation)->RemoteAddress;
			//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_ASSOCIATE_ADDRESS])...\n");
			break;
		case TDI_DISASSOCIATE_ADDRESS:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_DISASSOCIATE_ADDRESS])...\n");
			break;
		case TDI_CONNECT:
		 	pTDIRequestKernel=(PTDI_REQUEST_KERNEL )&IrpStack->Parameters;
            pTransAddr=(pTDIRequestKernel->RequestConnectionInformation)->RemoteAddress;
            //DbgPrint( "TAAddressCount: %d",pTransAddr->TAAddressCount );
            //DbgPrint( "AddressLength: %d",pTransAddr->Address[ 0 ].AddressLength );
            //DbgPrint( "AddressType: %d",pTransAddr->Address[ 0 ].AddressType );
            if(pTransAddr->Address[0 ].AddressType==TDI_ADDRESS_TYPE_IP)
			{
				pIPAddress = (PTDI_ADDRESS_IP )(PUCHAR )&pTransAddr->Address[0 ].Address;
                pByte = (PUCHAR)&pIPAddress->in_addr;
                DbgPrint( "in_addr: %d.%d.%d.%d",pByte[ 0 ], pByte[ 1 ], pByte[ 2 ], pByte[ 3 ] );
				DbgPrint( "sin_port: %d ",pIPAddress->sin_port);
				
				//event.ProcessID=ProcessId;				
				strcpy(event.Operation,"CONNECT");
				strcpy(event.ProcessName,ProcessName);
				addr1=(int)pByte[0];
				addr2=(int)pByte[1];
				addr3=(int)pByte[2];
				addr4=(int)pByte[3];
				sprintf(event.ProcessID,"%d",ProcessId);
				sprintf(event.port,"%d",(int)pIPAddress->sin_port);
                sprintf(event.addr1, "%d" , addr1);
				sprintf(event.addr2, "%d" , addr2);
				sprintf(event.addr3, "%d" , addr3);
				sprintf(event.addr4, "%d" , addr4);

				//Irp->Tail.Overlay.DriverContext[0]=(PVOID)&event;
				//pTDIH_DeviceExtension->pEvent=&event;
				//DebugPrintMsg(&event);
			}
			
			break;
		case TDI_DISCONNECT:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_DISCONNECT])...\n");
			break;
		case TDI_LISTEN:
			pConn=(PTDI_CONNECTION_INFORMATION )&IrpStack->Parameters;
		 	pTransAddr=pConn->RemoteAddress;
			DbgPrint("TDI_LISTEN");
			if(pTransAddr->Address[0 ].AddressType==TDI_ADDRESS_TYPE_IP)
			{
				pIPAddress = (PTDI_ADDRESS_IP )(PUCHAR )&pTransAddr->Address[0 ].Address;
                pByte = (PUCHAR)&pIPAddress->in_addr;
                DbgPrint( "in_addrXYZLISTEN: %d.%d.%d.%d",pByte[ 0 ], pByte[ 1 ], pByte[ 2 ], pByte[ 3 ] );
				DbgPrint( "sin_portXYZLISTEN: %d ",pIPAddress->sin_port);
			}
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_LISTEN])...\n");
			break;
		case TDI_QUERY_INFORMATION:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_QUERY_INFORMATION])...\n");
			break;
		case TDI_RECEIVE:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_RECEIVE])...\n");
			break;
		case TDI_RECEIVE_DATAGRAM:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_RECEIVE_DATAGRAM])...\n");
			break;
		case TDI_SEND:
			DbgPrint("TDI_SEND");
            
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_SEND])...\n");
			break;
		case TDI_SEND_DATAGRAM:
			DbgPrint("TDI_SEND_DATAGRAM");
            pConn=(PTDI_CONNECTION_INFORMATION )&IrpStack->Parameters;
		 	pTransAddr=pConn->RemoteAddress;
			if(pTransAddr->Address[0 ].AddressType==TDI_ADDRESS_TYPE_IP)
			{
				pIPAddress = (PTDI_ADDRESS_IP )(PUCHAR )&pTransAddr->Address[0 ].Address;
                pByte = (PUCHAR)&pIPAddress->in_addr;
                DbgPrint( "in_addrXYZSENDDATAGRAM: %d.%d.%d.%d",pByte[ 0 ], pByte[ 1 ], pByte[ 2 ], pByte[ 3 ] );
				DbgPrint( "sin_portXYZSENDDATAGRAM: %d ",pIPAddress->sin_port);
			}

		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_SEND_DATAGRAM])...\n");
			break;
		case TDI_SET_EVENT_HANDLER:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_SET_EVENT_HANDLER])...\n");
			break;
		case TDI_SET_INFORMATION:
		 	//DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL[TDI_SET_INFORMATION])...\n");
			break;
		default:
		 	DBGPRINT("PacketDispatch(IRP_MJ_INTERNAL_DEVICE_CONTROL\
				[INVALID_MINOR_FUNCTION])...\n");
			break;
		}
		break;
	case IRP_MJ_DEVICE_CONTROL:
		//DBGPRINT("PacketDispatch(IRP_MJ_DEVICE_CONTROL)...\n");
		break;
	default:
		DBGPRINT("PacketDispatch(OTHER_MAJOR_FUNCTION)...\n");
		break;
	}


	if (Irp->CurrentLocation == 1)
	{
		ULONG ReturnedInformation = 0;

		DBGPRINT(("PacketDispatch encountered bogus current location\n"));

		RC = STATUS_INVALID_DEVICE_REQUEST;
		Irp->IoStatus.Status = RC;
		Irp->IoStatus.Information = ReturnedInformation;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return( RC );
	}

	UTIL_IncrementLargeInteger(
		pTDIH_DeviceExtension->OutstandingIoRequests,
		(unsigned long)1,
		&(pTDIH_DeviceExtension->IoRequestsSpinLock)
		);
	KeClearEvent(&(pTDIH_DeviceExtension->IoInProgressEvent));

	
	NextIrpStack	= IoGetNextIrpStackLocation(Irp);
	//*NextIrpStack	= *IrpStack;
	IoCopyCurrentIrpStackLocationToNext(Irp);

    IoSetCompletionRoutine(Irp,PacketCompletion,pTDIH_DeviceExtension,TRUE,TRUE,TRUE);

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject,Irp);
}

NTSTATUS
PacketCompletion(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			Irp,
	IN	PVOID			Context
)
{
	PTDIH_DeviceExtension pTDIH_DeviceExtension;
   BOOLEAN           CanDetachProceed = FALSE;
   PDEVICE_OBJECT    pAssociatedDeviceObject = NULL;

   pTDIH_DeviceExtension = (PTDIH_DeviceExtension )(Context);

   ASSERT( pTDIH_DeviceExtension );

   //
   // Propogate The IRP Pending Flag
   //
   if (Irp->PendingReturned)
   {
      IoMarkIrpPending(Irp);
   }

   // Ensure that this is a valid device object pointer, else return
   // immediately.
   pAssociatedDeviceObject = pTDIH_DeviceExtension->pFilterDeviceObject;

   if (pAssociatedDeviceObject != pDeviceObject)
   {
      DBGPRINT(( "TDIH_DefaultCompletion: Invalid Device Object Pointer\n" ));
      return(STATUS_SUCCESS);
   }

   //if(Irp->Tail.Overlay.DriverContext[0]!=NULL)
   //{
	   //DebugPrintMsg((PEVENT)Irp->Tail.Overlay.DriverContext[0]);
	   //DbgPrint("PROCESSNAME: %s",((PEVENT)(Irp->Tail.Overlay.DriverContext[0]))->ProcessName);
   //}

   UTIL_DecrementLargeInteger(
      pTDIH_DeviceExtension->OutstandingIoRequests,
      (unsigned long)1,
      &(pTDIH_DeviceExtension->IoRequestsSpinLock)
      );

   UTIL_IsLargeIntegerZero(
      CanDetachProceed,
      pTDIH_DeviceExtension->OutstandingIoRequests,
      &(pTDIH_DeviceExtension->IoRequestsSpinLock)
      );

   if (CanDetachProceed)
   {
      KeSetEvent(&(pTDIH_DeviceExtension->IoInProgressEvent), IO_NO_INCREMENT, FALSE);
   }

   return(STATUS_SUCCESS);

}

NTSTATUS
TCPFilter_Attach(
	IN PDRIVER_OBJECT	DriverObject,
	IN PUNICODE_STRING	RegistryPath
)
{
	NTSTATUS				status	= 0;
	UNICODE_STRING			uniNtNameString;
	PTDIH_DeviceExtension	pTDIH_DeviceExtension;
	PDEVICE_OBJECT			pFilterDeviceObject = NULL;
	PDEVICE_OBJECT			pTargetDeviceObject = NULL;
	PFILE_OBJECT			pTargetFileObject	= NULL;
	PDEVICE_OBJECT			pLowerDeviceObject	= NULL;

	DBGPRINT("TCPFilter_Attach.\n");

	RtlInitUnicodeString( &uniNtNameString, DD_TCP_DEVICE_NAME );

	status = IoGetDeviceObjectPointer(
               IN	&uniNtNameString,
               IN	FILE_READ_ATTRIBUTES,
               OUT	&pTargetFileObject,   
               OUT	&pTargetDeviceObject
               );
	if( !NT_SUCCESS(status) )
	{
		DBGPRINT(("TCPFilter_Attach: Couldn't get the TCP Device Object\n"));
		pTargetFileObject	= NULL;
		pTargetDeviceObject = NULL;
		return( status );
	}

	RtlInitUnicodeString( &uniNtNameString, TDIH_TCP_DEVICE_NAME );

	status = IoCreateDevice(
               IN	DriverObject,
               IN	sizeof( TDIH_DeviceExtension ),
               IN	&uniNtNameString,
               IN	pTargetDeviceObject->DeviceType,
               IN	pTargetDeviceObject->Characteristics,
               IN	FALSE,                 
               OUT	&pFilterDeviceObject
               );
	if( !NT_SUCCESS(status) )
	{
		DBGPRINT(("TCPFilter_Attach: Couldn't create the TCP Filter Device Object\n"));
		ObDereferenceObject( pTargetFileObject );
		pTargetFileObject = NULL;
		pTargetDeviceObject = NULL;
		return( status );
	}

	pLowerDeviceObject 
		= IoAttachDeviceToDeviceStack(pFilterDeviceObject,pTargetDeviceObject);
	if( !pLowerDeviceObject )
	{
		DBGPRINT(("TCPFilter_Attach: Couldn't attach to TCP Device Object\n"));
		IoDeleteDevice( pFilterDeviceObject );
		pFilterDeviceObject = NULL;
		ObDereferenceObject( pTargetFileObject );
		pTargetFileObject	= NULL;
		pTargetDeviceObject = NULL;
		return( status );
	}

	pTDIH_DeviceExtension 
		= (PTDIH_DeviceExtension )( pFilterDeviceObject->DeviceExtension );
	TCPFilter_InitDeviceExtension(
		IN	pTDIH_DeviceExtension,
		IN	pFilterDeviceObject,
		IN	pTargetDeviceObject,
		IN	pTargetFileObject,
		IN	pLowerDeviceObject
		);

	pFilterDeviceObject->Flags |= pTargetDeviceObject->Flags 
		& (DO_BUFFERED_IO | DO_DIRECT_IO);
	return status;
}

NTSTATUS
TCPFilter_InitDeviceExtension(
	IN	PTDIH_DeviceExtension	pTDIH_DeviceExtension,
	IN	PDEVICE_OBJECT			pFilterDeviceObject,
	IN	PDEVICE_OBJECT			pTargetDeviceObject,
	IN	PFILE_OBJECT			pTargetFileObject,
	IN	PDEVICE_OBJECT			pLowerDeviceObject
)
{
	NdisZeroMemory( pTDIH_DeviceExtension, sizeof( TDIH_DeviceExtension ) );
	pTDIH_DeviceExtension->NodeType	= TDIH_NODE_TYPE_TCP_FILTER_DEVICE;
	pTDIH_DeviceExtension->NodeSize	= sizeof( TDIH_DeviceExtension );
	pTDIH_DeviceExtension->pFilterDeviceObject = pFilterDeviceObject;
	KeInitializeSpinLock(&(pTDIH_DeviceExtension->IoRequestsSpinLock));
	KeInitializeEvent(&(pTDIH_DeviceExtension->IoInProgressEvent)
		, NotificationEvent, FALSE);
	pTDIH_DeviceExtension->TargetDeviceObject	= pTargetDeviceObject;
	pTDIH_DeviceExtension->TargetFileObject		= pTargetFileObject;
	pTDIH_DeviceExtension->LowerDeviceObject	= pLowerDeviceObject;
	pTDIH_DeviceExtension->OutstandingIoRequests = 0;
	pTDIH_DeviceExtension->DeviceExtensionFlags |= TDIH_DEV_EXT_ATTACHED;
	//pTDIH_DeviceExtension->pEvent=NULL;
	//KeInitializeSpinLock(&(pTDIH_DeviceExtension->EventSpinLock));
	return( STATUS_SUCCESS );
}

VOID
TCPFilter_Detach(
	IN	PDEVICE_OBJECT pDeviceObject
)
{
	PTDIH_DeviceExtension	pTDIH_DeviceExtension;
	BOOLEAN					NoRequestsOutstanding = FALSE;

	pTDIH_DeviceExtension 
		= (PTDIH_DeviceExtension )pDeviceObject->DeviceExtension;
	try
	{
		try
		{
			while (TRUE)
			{
				UTIL_IsLargeIntegerZero(
					NoRequestsOutstanding,
					pTDIH_DeviceExtension->OutstandingIoRequests,
					&(pTDIH_DeviceExtension->IoRequestsSpinLock)
					);
				if( !NoRequestsOutstanding )
					KeWaitForSingleObject(
					(void *)(&(pTDIH_DeviceExtension->IoInProgressEvent)),
					Executive, KernelMode, FALSE, NULL
					);
				else
					break;
			}
         
			if( pTDIH_DeviceExtension->DeviceExtensionFlags 
				& TDIH_DEV_EXT_ATTACHED)
			{
				DBGPRINT(("DETACH XIAOYANG\n"));
				IoDetachDevice( pTDIH_DeviceExtension->TargetDeviceObject );
				pTDIH_DeviceExtension->DeviceExtensionFlags 
					&= ~(TDIH_DEV_EXT_ATTACHED);
			}

			pTDIH_DeviceExtension->NodeType = 0;
			pTDIH_DeviceExtension->NodeSize = 0;
			if( pTDIH_DeviceExtension->TargetFileObject )
				ObDereferenceObject( pTDIH_DeviceExtension->TargetFileObject );
			
			IoDeleteDevice( pDeviceObject );
            ClearEvents();
			DBGPRINT(("TCPFilter_Attach: TCPFilter_Detach Finished\n"));
		}
		except (EXCEPTION_EXECUTE_HANDLER){
		}
	}
	finally{}
	return;
}


//生成链表节点
void DebugPrintMsg(PEVENT event)
{
	ULONG TimeLen;
	ULONG EventDataLen;

	ULONG ProcessIdLen;
	ULONG ProcessNameLen;
	ULONG addr1Len;
	ULONG addr2Len;
	ULONG addr3Len;
	ULONG addr4Len;
	ULONG OperationLen;
	ULONG PortLen;

	ULONG len;

	LARGE_INTEGER Now,NowLocal;
	TIME_FIELDS NowTF;

	PDEBUGPRINT_EVENT pEvent;
	PUCHAR buffer;

//	char ProcessID[10];
//	char Port[8];

	if(DebugPrintStarted==FALSE || ExitNow==TRUE) return;

	if(event==NULL)return;
	//进行必要的格式转换
//   sprintf(ProcessID, "%d" , event->ProcessID);
//	sprintf(Port,"%d",event->port);

	KeQuerySystemTime(&Now);
	RtlTimeToTimeFields(&Now,&NowTF);

	//得到事件总体长度
	TimeLen=sizeof(TIME_FIELDS);

	ProcessIdLen=ANSIstrlen(event->ProcessID)+1;
	ProcessNameLen=ANSIstrlen(event->ProcessName)+1;
	addr1Len=ANSIstrlen(event->addr1)+1;
	addr2Len=ANSIstrlen(event->addr2)+1;
	addr3Len=ANSIstrlen(event->addr3)+1;
	addr4Len=ANSIstrlen(event->addr4)+1;
	OperationLen=ANSIstrlen(event->Operation)+1;
	PortLen=ANSIstrlen(event->port)+1;	

	//DbgPrint("addr1Len length: %d",addr1Len);
	//DbgPrint("addr1: %s",event->addr1);
	//DbgPrint("Process length: %d ProcessName length: %d",ProcessIdLen,ProcessNameLen);
	EventDataLen=TimeLen+ProcessIdLen+ProcessNameLen+addr1Len+addr2Len+addr3Len+addr4Len+OperationLen+PortLen;
	len=sizeof(LIST_ENTRY)+sizeof(ULONG)+EventDataLen;

	//分配事件缓冲区
	pEvent=(PDEBUGPRINT_EVENT)ExAllocatePool(NonPagedPool,len);
	if(pEvent!=NULL)
	{
		buffer=(PUCHAR)pEvent->EventData;
		RtlCopyMemory(buffer,&NowTF,TimeLen);
		buffer+=TimeLen;
		RtlCopyMemory(buffer,event->ProcessID,ProcessIdLen);
		buffer+=ProcessIdLen;
		RtlCopyMemory(buffer,event->ProcessName,ProcessNameLen);
		buffer+=ProcessNameLen;
		RtlCopyMemory(buffer,event->addr1,addr1Len);
		buffer+=addr1Len;
		RtlCopyMemory(buffer,event->addr2,addr2Len);
		buffer+=addr2Len;
		RtlCopyMemory(buffer,event->addr3,addr3Len);
		buffer+=addr3Len;
		RtlCopyMemory(buffer,event->addr4,addr4Len);
		buffer+=addr4Len;
        RtlCopyMemory(buffer,event->Operation,OperationLen);
		buffer+=OperationLen;
		RtlCopyMemory(buffer,event->port,PortLen);
		

		pEvent->Len=EventDataLen;
		ExInterlockedInsertTailList(&EventList,&pEvent->ListEntry,&EventListLock);
	}
	//ExFreePool(pEvent);

}
	

void DebugPrintInit(char* DriverName)
{
	//初始化全局变量
	HANDLE ThreadHandle;
	NTSTATUS status;

	ExitNow=FALSE;
	DebugPrintStarted=FALSE;
	ThreadObjectPointer=NULL;
	KeInitializeEvent(&ThreadEvent,SynchronizationEvent,FALSE);
	KeInitializeEvent(&ThreadExiting,SynchronizationEvent,FALSE);
	KeInitializeSpinLock(&EventListLock);
	InitializeListHead(&EventList);

	status=PsCreateSystemThread(&ThreadHandle,THREAD_ALL_ACCESS,NULL,NULL,NULL,DebugPrintSystemThread,NULL);
	if(!NT_SUCCESS(status))
	{
		DBGPRINT("FA SONG XIN XI SHI WU");
		return;
	}
	else
	{
		DBGPRINT("FA SONG XIN XI CHENG GONG");
	}
	status=ObReferenceObjectByHandle(ThreadHandle,THREAD_ALL_ACCESS,NULL,KernelMode,&ThreadObjectPointer,NULL);
	if(NT_SUCCESS(status))
		ZwClose(ThreadHandle);
}

void DebugPrintClose()
{
	DBGPRINT("YAO XIE ZAI LE");
	ExitNow=TRUE;
	KeSetEvent(&ThreadEvent,0,FALSE);
	KeWaitForSingleObject(&ThreadExiting,Executive,KernelMode,FALSE,NULL);
    DBGPRINT("XIE ZAI WAN LE");
}

void DebugPrintSystemThread(IN PVOID Context)
{
	UNICODE_STRING DebugPrintName;
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK IoStatus;
	HANDLE DebugPrintDeviceHandle;
	LARGE_INTEGER OneSecondTimeout;
	PDEBUGPRINT_EVENT pEvent;
	PLIST_ENTRY pListEntry;
	ULONG EventDataLen;
	NTSTATUS status;
	//LARGE_INTEGER ByteOffset;

	KeSetPriorityThread(KeGetCurrentThread(),LOW_REALTIME_PRIORITY);

	//创建文件
	
	RtlInitUnicodeString(&DebugPrintName,L"\\Device\\PHDDebugPrint");
	
	InitializeObjectAttributes(&ObjectAttributes,&DebugPrintName,OBJ_CASE_INSENSITIVE,NULL,NULL);
	
	DebugPrintDeviceHandle=NULL;
	status=ZwCreateFile(&DebugPrintDeviceHandle,
		GENERIC_READ|GENERIC_WRITE|SYNCHRONIZE,
		&ObjectAttributes,
		&IoStatus,
		0,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);
	if(!NT_SUCCESS(status)||DebugPrintDeviceHandle==NULL)
		goto exit1;

	//线程主循环代码，写文件

	DBGPRINT(("XIAN CHENG KAI SHI"));
	
	OneSecondTimeout.QuadPart=-1i64*1000000i64;

	DebugPrintStarted=TRUE;

	while(TRUE)
	{
		KeWaitForSingleObject(&ThreadEvent,Executive,KernelMode,FALSE,&OneSecondTimeout);
		
		while(TRUE)
		{
			pListEntry=ExInterlockedRemoveHeadList(&EventList,&EventListLock);
			if(pListEntry==NULL)
			{
				//DBGPRINT(("WOKAOHAIMEILIANSHANG"));
				break;
			}
			else
			{
				DBGPRINT(("YOU QING KUANG"));
			}

			pEvent=
				CONTAINING_RECORD(pListEntry,DEBUGPRINT_EVENT,ListEntry);
			EventDataLen=pEvent->Len;

			status=ZwWriteFile(DebugPrintDeviceHandle,NULL,NULL,NULL,
				&IoStatus,pEvent->EventData,EventDataLen,NULL,NULL);

			if(!NT_SUCCESS(status))
			{
				DBGPRINT("MEI FA XIE A");
			}
			else
			{
				DBGPRINT("WO XIE!");
			}
			ExFreePool(pEvent);
		}

	if(ExitNow)
		break;
	}
exit1:
	if(ThreadObjectPointer!=NULL)
	{
		ObDereferenceObject(&ThreadObjectPointer);
		ThreadObjectPointer=NULL;
	}

	/////////////////////////////////////
	//有待商榷
	DebugPrintStarted=FALSE;
	
	ZwClose(DebugPrintDeviceHandle);
	/////////////////////////////////////

	KeSetEvent(&ThreadExiting,0,FALSE);
	
	PsTerminateSystemThread(STATUS_SUCCESS);	
}

void ClearEvents()
{
	PLIST_ENTRY pListEntry;
	PDEBUGPRINT_EVENT pEvent;
	DBGPRINT("SHAN CHU KAI SHI");
	while(TRUE)
	{
		pListEntry=ExInterlockedRemoveHeadList(&EventList,&EventListLock);
		if(pListEntry==NULL)
			break;

	    pEvent=
			CONTAINING_RECORD(pListEntry,DEBUGPRINT_EVENT,ListEntry);
				
		ExFreePool(pEvent);
	}
	DBGPRINT("SHAN CHU WAN BI");
}
USHORT ANSIstrlen( char* str)
{
	USHORT len = 0;
	for(;*str++!='\0';)
		len++;
	return len;
}
