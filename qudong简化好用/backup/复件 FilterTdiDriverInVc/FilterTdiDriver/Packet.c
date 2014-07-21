
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

	//得到进程偏移号
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

	//卸载例程
	DriverObject->DriverUnload = TDIH_Unload;

	//文件列表初始化
    KeInitializeSpinLock(&FileObjectLock);
	InitializeListHead(&FileObjectList);

	//加载信息输出模块
	DebugPrintInit("FilterTdiDriver");

	//普通分发例程
    for (i=0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
       DriverObject->MajorFunction[i] = TDIH_DefaultDispatch;
    }

	//挂接TCP设备
	status = TCPFilter_Attach( DriverObject, RegistryPath );

	//挂接UDP设备
    status = UDPFilter_Attach( DriverObject, RegistryPath );

	return status;
}

VOID 
TDIH_Unload(
	IN PDRIVER_OBJECT		DriverObject
)
{
    PDEVICE_OBJECT			DeviceObject;
	PDEVICE_OBJECT          pNextDevice;
	PTDIH_DeviceExtension	pTDIH_DeviceExtension;	

 	DBGPRINT("DriverEntry unLoading...\n");

	DeviceObject = DriverObject->DeviceObject;

	//卸载信息输出模块
	DebugPrintClose();

    while (DeviceObject != NULL) 
	{
		pNextDevice=DeviceObject->NextDevice;

		pTDIH_DeviceExtension	
			= (PTDIH_DeviceExtension )DeviceObject->DeviceExtension;
		if( pTDIH_DeviceExtension->NodeType 
			== TDIH_NODE_TYPE_TCP_FILTER_DEVICE )
		{
			TCPFilter_Detach( DeviceObject );   
		}
		else if(pTDIH_DeviceExtension->NodeType 
			== TDIH_NODE_TYPE_UDP_FILTER_DEVICE )
		{
			UDPFilter_Detach( DeviceObject );
		}
	
        DeviceObject = pNextDevice;
    }   
	//删除事件链表缓冲区
	ClearEvents();
	//删除文件列表缓冲区
	TDIH_DeleteAllFileObjectNodes();
}

NTSTATUS
TDIH_DefaultDispatch(
    IN PDEVICE_OBJECT		DeviceObject,
    IN PIRP					Irp
)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PTDIH_DeviceExtension	pTDIH_DeviceExtension;
	PIO_STACK_LOCATION		IrpStack;
	PIO_STACK_LOCATION		NextIrpStack;

	pTDIH_DeviceExtension	
		= (PTDIH_DeviceExtension )(DeviceObject->DeviceExtension);

	IrpStack = IoGetCurrentIrpStackLocation(Irp);

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

    IoSetCompletionRoutine(Irp,DefaultDispatchCompletion,pTDIH_DeviceExtension,TRUE,TRUE,TRUE);

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject,Irp);
}

NTSTATUS
DefaultDispatchCompletion(
	IN	PDEVICE_OBJECT	pDeviceObject,
	IN	PIRP			Irp,
	IN	PVOID			Context
)
{
   PTDIH_DeviceExtension pTDIH_DeviceExtension;
   BOOLEAN           CanDetachProceed = FALSE;
   PDEVICE_OBJECT    pAssociatedDeviceObject = NULL;
   PIO_STACK_LOCATION		IrpStack;

   ////////////////////////////////////////////////
   //通信参数
   PTDI_REQUEST_KERNEL pTDIRequestKernel;
   PTDI_ADDRESS_IP pIPAddress;
   PTRANSPORT_ADDRESS pTransAddr;

   PUCHAR pByte;
   EVENT event;

   int addr1,addr2,addr3,addr4;

   ULONG ProcessId;
   CHAR       ProcessName[16];
   PEPROCESS   curproc;
   int         i;
   char        *nameptr;

   PFILEOBJECT_NODE pFileObjectNode;
   //////////////////////////////////////////////////

   pTDIH_DeviceExtension = (PTDIH_DeviceExtension )(Context);

   ASSERT( pTDIH_DeviceExtension );

   if (Irp->PendingReturned)
   {
      IoMarkIrpPending(Irp);
	  //strcpy(event.SuccOrFail,"PENDING");
   }

   pAssociatedDeviceObject = pTDIH_DeviceExtension->pFilterDeviceObject;

   if (pAssociatedDeviceObject != pDeviceObject)
   {
      DBGPRINT(( "TDIH_DefaultCompletion: Invalid Device Object Pointer\n" ));
      return(STATUS_SUCCESS);
   }

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

   //操作是否成功
   if(NT_SUCCESS(Irp->IoStatus.Status))
   {
	   strcpy(event.SuccOrFail,"SUCCESSFUL");
   }
   if(!NT_SUCCESS(Irp->IoStatus.Status))
   {
	   strcpy(event.SuccOrFail,"UNSUCCESSFUL");
   }

   if (Irp->PendingReturned)
   {
	  strcpy(event.SuccOrFail,"PENDING");
   }
   //进程名
   strcpy(event.ProcessName,ProcessName);

   //进程号
   sprintf(event.ProcessID,"%d",ProcessId);

   //IP地址和端口号
   strcpy(event.addr1,"");
   strcpy(event.addr2,"");
   strcpy(event.addr3,"");
   strcpy(event.addr4,"");
   strcpy(event.port,"");
	   
   //操作
   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
   {		  
	   strcpy(event.Operation,"TCP");	   
   }
   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
   {		   
	   strcpy(event.Operation,"UDP");	 
   }
   else
   {
	   strcpy(event.Operation,"OTHER");
   }
   DebugPrintMsg(&event);
/*
   switch(IrpStack->MajorFunction)
   {
   case IRP_MJ_CREATE:
	   strcpy(event.addr1,"");
	   strcpy(event.addr2,"");
	   strcpy(event.addr3,"");
	   strcpy(event.addr4,"");
	   strcpy(event.port,"");
	   
	   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
	   {
		   strcpy(event.Operation,"TCPCREAT");
	   }
	   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
	   {
		   strcpy(event.Operation,"UDPCREAT");
	   }

	   //发送信息
	   DebugPrintMsg(&event);

	   TDIH_Create(pTDIH_DeviceExtension, Irp, IrpStack);
	   break;
   case IRP_MJ_CLOSE:
	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
       if(pFileObjectNode==NULL)
	   {
		   strcpy(event.addr1,"");
	       strcpy(event.addr2,"");
	       strcpy(event.addr3,"");
	       strcpy(event.addr4,"");
	       strcpy(event.port,"");
	   
	       if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
		   {
		       strcpy(event.Operation,"TCPCLOSE");
		   }
	       else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
		   {
		       strcpy(event.Operation,"UDPCLOSE");
		   }
		   else
		   {
		       strcpy(event.Operation,"CREAT");
		   }
		   DbgPrint("THERE IS SOMETHINT WRONG IN CLOSE: THERE IS NO RECORD");
	   }
	   else
	   {
		   if(pFileObjectNode->SET==FALSE)
		   {
			   strcpy(event.addr1,"");
			   strcpy(event.addr2,"");
			   strcpy(event.addr3,"");
			   strcpy(event.addr4,"");
			   strcpy(event.port,"");					   
		   }
		   else
		   {
			   pIPAddress=pFileObjectNode->IP;
			   pByte = (PUCHAR)&pIPAddress->in_addr;

			   addr1=(int)pByte[0];
			   addr2=(int)pByte[1];
			   addr3=(int)pByte[2];
			   addr4=(int)pByte[3];

			   //IP地址
			   sprintf(event.addr1, "%d" , addr1);
			   sprintf(event.addr2, "%d" , addr2);
			   sprintf(event.addr3, "%d" , addr3);
			   sprintf(event.addr4, "%d" , addr4);

			   //端口号
			   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
			}

		    //操作类型
			if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
			{
				strcpy(event.Operation,"TCP---CLOSE");
			}
			else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
			{
				strcpy(event.Operation,"UDP---CLOSE");
			}
	   }	

	   //发送信息
	   DebugPrintMsg(&event);

	   TDIH_Close(pTDIH_DeviceExtension, Irp, IrpStack); 
       break;

   case IRP_MJ_CLEANUP:
	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
       if(pFileObjectNode==NULL)
	   {
		   strcpy(event.addr1,"");
	       strcpy(event.addr2,"");
	       strcpy(event.addr3,"");
	       strcpy(event.addr4,"");
	       strcpy(event.port,"");
	   
	       if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
		   {
		       strcpy(event.Operation,"TCP---CLEANUP");
		   }
	       else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
		   {
		       strcpy(event.Operation,"UDP---CLEANUP");
		   }
		   
		   DbgPrint("THERE IS SOMETHINT WRONG IN CLEANUP: THERE IS NO RECORD");
	   }
	   else
	   {
		   if(pFileObjectNode->SET==FALSE)
		   {
			   strcpy(event.addr1,"");
			   strcpy(event.addr2,"");
			   strcpy(event.addr3,"");
			   strcpy(event.addr4,"");
			   strcpy(event.port,"");					   
		   }
		   else
		   {
			   pIPAddress=pFileObjectNode->IP;
			   pByte = (PUCHAR)&pIPAddress->in_addr;

			   addr1=(int)pByte[0];
			   addr2=(int)pByte[1];
			   addr3=(int)pByte[2];
			   addr4=(int)pByte[3];

			   //IP地址
			   sprintf(event.addr1, "%d" , addr1);
			   sprintf(event.addr2, "%d" , addr2);
			   sprintf(event.addr3, "%d" , addr3);
			   sprintf(event.addr4, "%d" , addr4);

			   //端口号
			   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
			}

		    //操作类型
			if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
			{
				strcpy(event.Operation,"TCP---CLEANUP");
			}
			else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
			{
				strcpy(event.Operation,"UDP---CLEANUP");
			}
	   }	

	   //发送信息
	   DebugPrintMsg(&event);

	   TDIH_CleanUp(pTDIH_DeviceExtension, Irp, IrpStack);
	   break;
   case IRP_MJ_INTERNAL_DEVICE_CONTROL:
	   switch(IrpStack->MinorFunction)
	   {
		   case TDI_ACCEPT:
			   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   strcpy(event.addr1,"");
	               strcpy(event.addr2,"");
	               strcpy(event.addr3,"");
	               strcpy(event.addr4,"");
	               strcpy(event.port,"");
	   
	               if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
		               strcpy(event.Operation,"TCP---TDI_ACCEPT");
				   }
	               else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
		               strcpy(event.Operation,"UDP---TDI_ACCEPT");
				   }
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_ACCEPT: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_ACCEPT");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_ACCEPT");
				   }
			   }	
			   
			   //发送信息
	           DebugPrintMsg(&event);

			   break;
/*	
	   case TDI_ACTION:
			   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_ACTION: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_ACTION");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_ACTION");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
		 	   break;
		   case TDI_ASSOCIATE_ADDRESS:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_ASSOCIATE_ADDRESS: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_ASSOCIATE_ADDRESS");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_ASSOCIATE_ADDRESS");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_DISASSOCIATE_ADDRESS:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_DISASSOCIATE_ADDRESS: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_DISASSOCIATE_ADDRESS");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_DISASSOCIATE_ADDRESS");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_CONNECT:
			   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_CONNECT: THERE IS NO RECORD");
			   }
			   else
			   {
				   pTDIRequestKernel=(PTDI_REQUEST_KERNEL )&IrpStack->Parameters;
                   pTransAddr=(pTDIRequestKernel->RequestConnectionInformation)->RemoteAddress;
               
				   if(pTransAddr->Address[0 ].AddressType==TDI_ADDRESS_TYPE_IP)
				   {
				       pIPAddress = (PTDI_ADDRESS_IP )(PUCHAR )&pTransAddr->Address[0 ].Address;
                       pByte = (PUCHAR)&pIPAddress->in_addr;
                   		
			           strcpy(event.Operation,"CONNECT");
			      
			           addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];
			      
			           sprintf(event.port,"%d",(USHORT)pIPAddress->sin_port);
                       sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //写入文件链表地址和端口号内容
                       pFileObjectNode->IP=pIPAddress;
					   pFileObjectNode->PORT=(USHORT)pIPAddress->sin_port;
					   pFileObjectNode->SET=TRUE;

					   //发送信息
			           DebugPrintMsg(&event);
				   }
			   }
		       break;
		   case TDI_DISCONNECT:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_DISCONNECT: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_DISCONNECT");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_DISCONNECT");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_LISTEN:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_LISTEN: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_LISTEN");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_LISTEN");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;

  	       case TDI_QUERY_INFORMATION:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_QUERY_INFORMATION: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_QUERY_INFORMATION");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_QUERY_INFORMATION");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_RECEIVE:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_RECEIVE: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_RECEIVE");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_RECEIVE");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
*/
/*
		   case TDI_RECEIVE_DATAGRAM:
		       pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_RECEIVE_DATAGRAM: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_RECEIVE_DATAGRAM");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_RECEIVE_DATAGRAM");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_SEND:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_SEND: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_SEND");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_SEND");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_SEND_DATAGRAM:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_SEND_DATAGRAM: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_SEND_DATAGRAM");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_SEND_DATAGRAM");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_SET_EVENT_HANDLER:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_SET_EVENT_HANDLER: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_SET_EVENT_HANDLER");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_SET_EVENT_HANDLER");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
		   case TDI_SET_INFORMATION:
		 	   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_SET_INFORMATION: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"");
					   strcpy(event.addr2,"");
					   strcpy(event.addr3,"");
					   strcpy(event.addr4,"");
					   strcpy(event.port,"");					   
				   }
				   else
				   {
                       pIPAddress=pFileObjectNode->IP;
					   pByte = (PUCHAR)&pIPAddress->in_addr;

					   addr1=(int)pByte[0];
			           addr2=(int)pByte[1];
			           addr3=(int)pByte[2];
			           addr4=(int)pByte[3];

					   //IP地址
					   sprintf(event.addr1, "%d" , addr1);
			           sprintf(event.addr2, "%d" , addr2);
			           sprintf(event.addr3, "%d" , addr3);
			           sprintf(event.addr4, "%d" , addr4);

					   //端口号
					   sprintf(event.port,"%d",(int)pIPAddress->sin_port);
				   }

				   //操作类型
				   if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"TCP---TDI_SET_INFORMATION");
				   }
				   else if(pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
				   {
					   strcpy(event.Operation,"UDP---TDI_SET_INFORMATION");
				   }

				   //发送信息
	               DebugPrintMsg(&event);
			   }
			   break;
*/	
/*	   
		   default:
		 	   DebugPrintMsg(&event);
			   break;
	   }
   default:
	   DebugPrintMsg(&event);
	   break;
   }
*/
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

VOID
TDIH_Create(
   PTDIH_DeviceExtension   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpStack
   )
{
/*
   NTSTATUS                            RC;
   FILE_FULL_EA_INFORMATION            *ea;
   
   PFILEOBJECT_NODE                    pFileObjectNode;
   PFILEOBJECT_NODE                    pGotFileObject;


   ea = (PFILE_FULL_EA_INFORMATION) Irp->AssociatedIrp.SystemBuffer;

   if (!ea)
   {
      DbgPrint("Can Not Create!\n");
   }

   pFileObjectNode=(PFILEOBJECT_NODE)ExAllocatePool(NonPagedPool,sizeof(FILEOBJECT_NODE));
   pFileObjectNode->pFileObject=IrpStack->FileObject;
   pGotFileObject=TDIH_GetFileObjectFromList(pFileObjectNode->pFileObject);
   if(pGotFileObject!=NULL)
   {
	   DbgPrint("FileObject Has Existed!");
	   ExFreePool(pFileObjectNode);
	   return;
   }
   else
   {
	   pFileObjectNode->SET=FALSE;
	   ExInterlockedInsertTailList(&FileObjectList,&pFileObjectNode->ListEntry,&FileObjectLock);
   }
   return;
*/
}

VOID
TDIH_Close(
   PTDIH_DeviceExtension   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpStack
   )
{
	/*
	PFILEOBJECT_NODE pFileObjectNode;
	if((int) IrpStack->FileObject->FsContext2 == TDI_TRANSPORT_ADDRESS_FILE)
	{
        pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
        if(pFileObjectNode!=NULL)
		{
			DbgPrint("THE FILEOBJECT RECORD STILL EXIST!");
			return;
		}
	}
	*/
}

VOID
TDIH_CleanUp(
   PTDIH_DeviceExtension   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpStack
   )
{
	/*
	KIRQL            OldIrql;

	PFILEOBJECT_NODE pFileObjectNode;
	if((int) IrpStack->FileObject->FsContext2 == TDI_TRANSPORT_ADDRESS_FILE)
	{
        pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
        if(pFileObjectNode==NULL)
		{
			DbgPrint("THERE IS SOMETHING WRONG: NO FILEOBJECT RECORD");
			return;
		}
		else
		{                                                                     
            KeAcquireSpinLock(&FileObjectLock, &OldIrql); 
			RemoveEntryList(&pFileObjectNode->ListEntry);
			KeReleaseSpinLock(&FileObjectLock, OldIrql);

			ExFreePool(pFileObjectNode);
		}
	}
	*/
}

PFILEOBJECT_NODE
TDIH_GetFileObjectFromList(PFILE_OBJECT pFileObject)
{
	/*
	PFILEOBJECT_NODE pFileObjectNode;
	PLIST_ENTRY pdLink ;
 
	KIRQL            OldIrql;                                                                     
    KeAcquireSpinLock(&FileObjectLock, &OldIrql);                                                   
   
	if(!IsListEmpty( &FileObjectList))
	{
	    pdLink =FileObjectList.Flink;		
	}
	else
	{
		return( (PFILEOBJECT_NODE)NULL );
	}

	while( !IsListEmpty( &FileObjectList)
		&& pdLink != (PLIST_ENTRY )(&FileObjectList))
	{	
		pFileObjectNode=CONTAINING_RECORD(pdLink,FILEOBJECT_NODE,ListEntry);

		if( pFileObjectNode->pFileObject == pFileObject )
		{
			return( pFileObjectNode );
		}

		pdLink = pdLink->Flink;
	}

    KeReleaseSpinLock(&FileObjectLock, OldIrql);
	*/
    return( (PFILEOBJECT_NODE)NULL );
}

VOID
TDIH_DeleteAllFileObjectNodes()
{
	/*
	PLIST_ENTRY             pListEntry;
	PFILEOBJECT_NODE        pFileObjectNode;

	while(TRUE)
	{
		pListEntry=ExInterlockedRemoveHeadList(&FileObjectList,&FileObjectLock);
		if(pListEntry==NULL)
			break;

	    pFileObjectNode=
			CONTAINING_RECORD(pListEntry,FILEOBJECT_NODE,ListEntry);
				
		ExFreePool(pFileObjectNode);
	}
*/
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
UDPFilter_Attach(
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

	DBGPRINT("UDPFilter_Attach.\n");

	RtlInitUnicodeString( &uniNtNameString, DD_UDP_DEVICE_NAME );

	status = IoGetDeviceObjectPointer(
               IN	&uniNtNameString,
               IN	FILE_READ_ATTRIBUTES,
               OUT	&pTargetFileObject,   
               OUT	&pTargetDeviceObject
               );
	if( !NT_SUCCESS(status) )
	{
		DBGPRINT(("UDPFilter_Attach: Couldn't get the UDP Device Object\n"));
		pTargetFileObject	= NULL;
		pTargetDeviceObject = NULL;
		return( status );
	}

	RtlInitUnicodeString( &uniNtNameString, TDIH_UDP_DEVICE_NAME );

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
		DBGPRINT(("UDPFilter_Attach: Couldn't create the UDP Filter Device Object\n"));
		ObDereferenceObject( pTargetFileObject );
		pTargetFileObject = NULL;
		pTargetDeviceObject = NULL;
		return( status );
	}

	pLowerDeviceObject 
		= IoAttachDeviceToDeviceStack(pFilterDeviceObject,pTargetDeviceObject);
	if( !pLowerDeviceObject )
	{
		DBGPRINT(("UDPFilter_Attach: Couldn't attach to UDP Device Object\n"));
		IoDeleteDevice( pFilterDeviceObject );
		pFilterDeviceObject = NULL;
		ObDereferenceObject( pTargetFileObject );
		pTargetFileObject	= NULL;
		pTargetDeviceObject = NULL;
		return( status );
	}

	pTDIH_DeviceExtension 
		= (PTDIH_DeviceExtension )( pFilterDeviceObject->DeviceExtension );
	UDPFilter_InitDeviceExtension(
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
	return( STATUS_SUCCESS );
}

NTSTATUS
UDPFilter_InitDeviceExtension(
	IN	PTDIH_DeviceExtension	pTDIH_DeviceExtension,
	IN	PDEVICE_OBJECT			pFilterDeviceObject,
	IN	PDEVICE_OBJECT			pTargetDeviceObject,
	IN	PFILE_OBJECT			pTargetFileObject,
	IN	PDEVICE_OBJECT			pLowerDeviceObject
)
{
	NdisZeroMemory( pTDIH_DeviceExtension, sizeof( TDIH_DeviceExtension ) );
	pTDIH_DeviceExtension->NodeType	= TDIH_NODE_TYPE_UDP_FILTER_DEVICE;
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
	ASSERT(pTDIH_DeviceExtension);

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
				IoDetachDevice( pTDIH_DeviceExtension->TargetDeviceObject );
				pTDIH_DeviceExtension->DeviceExtensionFlags 
					&= ~(TDIH_DEV_EXT_ATTACHED);
			}

			pTDIH_DeviceExtension->NodeType = 0;
			pTDIH_DeviceExtension->NodeSize = 0;
			if( pTDIH_DeviceExtension->TargetFileObject )
			{
				ObDereferenceObject( pTDIH_DeviceExtension->TargetFileObject );
			}
			pTDIH_DeviceExtension->TargetFileObject=NULL;
			IoDeleteDevice( pDeviceObject );
            
			DBGPRINT(("TCPFilter_Attach: TCPFilter_Detach Finished\n"));
		}
		except (EXCEPTION_EXECUTE_HANDLER){
		}
	}
	finally{}
	return;
}

VOID
UDPFilter_Detach(
	IN	PDEVICE_OBJECT pDeviceObject
)
{
	PTDIH_DeviceExtension	pTDIH_DeviceExtension;
	BOOLEAN					NoRequestsOutstanding = FALSE;

	pTDIH_DeviceExtension 
		= (PTDIH_DeviceExtension )pDeviceObject->DeviceExtension;
	ASSERT(pTDIH_DeviceExtension);

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
				IoDetachDevice( pTDIH_DeviceExtension->TargetDeviceObject );
				pTDIH_DeviceExtension->DeviceExtensionFlags 
					&= ~(TDIH_DEV_EXT_ATTACHED);
			}

			pTDIH_DeviceExtension->NodeType = 0;
			pTDIH_DeviceExtension->NodeSize = 0;
			if( pTDIH_DeviceExtension->TargetFileObject )
			{
				ObDereferenceObject( pTDIH_DeviceExtension->TargetFileObject );
			}
			pTDIH_DeviceExtension->TargetFileObject=NULL;
			IoDeleteDevice( pDeviceObject );
            
			DBGPRINT(("UDPFilter_Attach: UDPFilter_Detach Finished\n"));
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
	ULONG ResultLen;

	ULONG len;

	LARGE_INTEGER Now,NowLocal;
	TIME_FIELDS NowTF;

	PDEBUGPRINT_EVENT pEvent;
	PUCHAR buffer;

	if(DebugPrintStarted==FALSE || ExitNow==TRUE) return;

	if(event==NULL)return;

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
	ResultLen=ANSIstrlen(event->SuccOrFail)+1;

	EventDataLen=TimeLen+ProcessIdLen+ProcessNameLen+addr1Len+addr2Len+addr3Len+addr4Len+OperationLen+PortLen+ResultLen;
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
		buffer+=PortLen;
		RtlCopyMemory(buffer,event->SuccOrFail,ResultLen);
		
		pEvent->Len=EventDataLen;
		ExInterlockedInsertTailList(&EventList,&pEvent->ListEntry,&EventListLock);
	}
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
				break;
			}
			else
			{
			}

			pEvent=
				CONTAINING_RECORD(pListEntry,DEBUGPRINT_EVENT,ListEntry);
			EventDataLen=pEvent->Len;

			status=ZwWriteFile(DebugPrintDeviceHandle,NULL,NULL,NULL,
				&IoStatus,pEvent->EventData,EventDataLen,NULL,NULL);

			if(!NT_SUCCESS(status))
			{
			}
			else
			{
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
	
	while(TRUE)
	{
		pListEntry=ExInterlockedRemoveHeadList(&EventList,&EventListLock);
		if(pListEntry==NULL)
			break;

	    pEvent=
			CONTAINING_RECORD(pListEntry,DEBUGPRINT_EVENT,ListEntry);
				
		ExFreePool(pEvent);
	}
	
}
USHORT ANSIstrlen( char* str)
{
	USHORT len = 0;
	for(;*str++!='\0';)
		len++;
	return len;
}
