
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

	//文件列表及锁初始化
    KeInitializeSpinLock(&FileObjectLock);
	InitializeListHead(&FileObjectList);
	KeInitializeSpinLock(&PROTECTLOCK);
	
	status = TCPFilter_Attach( DriverObject, RegistryPath );

    status = UDPFilter_Attach( DriverObject, RegistryPath );

	//加载信息输出模块
	DebugPrintInit("FilterTdiDriver");

   for (i=0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
   {
      DriverObject->MajorFunction[i] = TDIH_DefaultDispatch;
   }

   return status;
}

VOID 
TDIH_Unload(
	IN PDRIVER_OBJECT		DriverObject
)
{
   PDEVICE_OBJECT             pDeviceObject;
   PDEVICE_OBJECT             pNextDeviceObject;
   PTDIH_DeviceExtension pTDIH_DeviceExtension;

   pDeviceObject = DriverObject->DeviceObject;

   	//卸载信息输出模块
	DebugPrintClose();

	DbgPrint("DEBUGPRINT DETACH FINISHED");

   while( pDeviceObject != NULL )
   {
      pNextDeviceObject = pDeviceObject->NextDevice;

      pTDIH_DeviceExtension = (PTDIH_DeviceExtension )pDeviceObject->DeviceExtension;

      if( pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE )
      {
         TCPFilter_Detach( pDeviceObject );   
      }
      else if( pTDIH_DeviceExtension->NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE )
      {
         UDPFilter_Detach( pDeviceObject );   
      }

      pDeviceObject = pNextDeviceObject;
   }

   //删除事件链表缓冲区
   ClearEvents();
   //删除文件链表缓冲区
   TDIH_DeleteAllFileObjectNodes();
}

NTSTATUS
TDIH_DefaultDispatch(
    IN PDEVICE_OBJECT		DeviceObject,
    IN PIRP					Irp
)
{
	NTSTATUS                RC = STATUS_SUCCESS;
   PIO_STACK_LOCATION      IrpSp = NULL;
   PTDIH_DeviceExtension   pTDIH_DeviceExtension;
   PDEVICE_OBJECT          pLowerDeviceObject = NULL;
	
   pTDIH_DeviceExtension = (PTDIH_DeviceExtension )(DeviceObject->DeviceExtension);

   IrpSp = IoGetCurrentIrpStackLocation(Irp);
   ASSERT(IrpSp);

   try
   {
      pLowerDeviceObject = pTDIH_DeviceExtension->LowerDeviceObject;

      if (Irp->CurrentLocation == 1)
      {
         ULONG ReturnedInformation = 0;

         RC = STATUS_INVALID_DEVICE_REQUEST;
         Irp->IoStatus.Status = RC;
         Irp->IoStatus.Information = ReturnedInformation;
         IoCompleteRequest(Irp, IO_NO_INCREMENT);

         return( RC );
      }

      IoCopyCurrentIrpStackLocationToNext( Irp );
     
      IoSetCompletionRoutine(
         Irp,
         DefaultDispatchCompletion,
         pTDIH_DeviceExtension,
         TRUE,
         TRUE,
         TRUE
         );
      
	  UTIL_IncrementLargeInteger(
         pTDIH_DeviceExtension->OutstandingIoRequests,
         (unsigned long)1,
         &(pTDIH_DeviceExtension->IoRequestsSpinLock)
         );

      KeClearEvent(&(pTDIH_DeviceExtension->IoInProgressEvent));

      RC = IoCallDriver(pLowerDeviceObject, Irp);
   }

   finally
   {
   }

   return(RC);

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
	  strcpy(event.SuccOrFail,"PENDING");
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

   //进程名
   strcpy(event.ProcessName,ProcessName);

   //进程号
   sprintf(event.ProcessID,"%d",ProcessId);

   switch(IrpStack->MajorFunction)
   {
   case IRP_MJ_CREATE:

	   strcpy(event.addr1,"*");
	   strcpy(event.addr2,"*");
	   strcpy(event.addr3,"*");
	   strcpy(event.addr4,"*");
	   strcpy(event.port,"*");

       if(pTDIH_DeviceExtension->NodeType==TDIH_NODE_TYPE_TCP_FILTER_DEVICE)
	   {
	       strcpy(event.Operation,"TCP CREATE");
	   }
       else if(pTDIH_DeviceExtension->NodeType==TDIH_NODE_TYPE_UDP_FILTER_DEVICE)
	   {
	       strcpy(event.Operation,"UDP CREATE");
	   }
       else
	   {
	       strcpy(event.Operation,"OTHER CREATE");
	   }

	   DebugPrintMsg(&event);

	   TDIH_Create(pTDIH_DeviceExtension, Irp, IrpStack);
	   break;
   case IRP_MJ_CLOSE:
	   
       pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
       if(pFileObjectNode==NULL)
	   {
		   DbgPrint("THERE IS SOMETHINT WRONG IN CLOSE: THERE IS NO RECORD");
	   }
	   else
	   {
           if(pFileObjectNode->SET==FALSE)
		   {                 
			   strcpy(event.addr1,"*");
			   strcpy(event.addr2,"*");					   			   
			   strcpy(event.addr3,"*");					  
			   strcpy(event.addr4,"*");					  
			   strcpy(event.port,"*");					   				 
		   }				 
		   else				  
		   {					
			   //IP地址					  
			   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);			         
			   sprintf(event.addr2, "%d" , pFileObjectNode->addr2);			         
			   sprintf(event.addr3, "%d" , pFileObjectNode->addr3);			       
			   sprintf(event.addr4, "%d" , pFileObjectNode->addr4);                       
					
			   //端口号					  
			   sprintf(event.port,"%d",pFileObjectNode->port);				  
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
		   else				
		   {					  
			   strcpy(event.Operation,"OTHER---CLOSE");				  
		   }				   
		   //发送信息			       
		   DebugPrintMsg(&event);			  
	   }
	   
	   TDIH_Close(pTDIH_DeviceExtension, Irp, IrpStack); 
	   break;
   case IRP_MJ_CLEANUP:
	   
       pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
       if(pFileObjectNode==NULL)
	   {
		   DbgPrint("THERE IS SOMETHINT WRONG IN CLEANUP: THERE IS NO RECORD");
	   }
	   else
	   {
           if(pFileObjectNode->SET==FALSE)
		   {                 
			   strcpy(event.addr1,"*");
			   strcpy(event.addr2,"*");					   			   
			   strcpy(event.addr3,"*");					  
			   strcpy(event.addr4,"*");					  
			   strcpy(event.port,"*");					   				 
		   }				 
		   else				  
		   {					
			   //IP地址					  
			   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);			         
			   sprintf(event.addr2, "%d" , pFileObjectNode->addr2);			         
			   sprintf(event.addr3, "%d" , pFileObjectNode->addr3);			       
			   sprintf(event.addr4, "%d" , pFileObjectNode->addr4);                       
					
			   //端口号					  
			   sprintf(event.port,"%d",pFileObjectNode->port);				  
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
		   else				
		   {					  
			   strcpy(event.Operation,"OTHER---CLEANUP");				  
		   }				   
		   //发送信息			       
		   DebugPrintMsg(&event);			  
	   }

	   TDIH_CleanUp(pTDIH_DeviceExtension, Irp, IrpStack);
	   break;
   case IRP_MJ_INTERNAL_DEVICE_CONTROL:
	   switch(IrpStack->MinorFunction)
	   {
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
                       pFileObjectNode->addr1=addr1;
					   pFileObjectNode->addr2=addr2;
					   pFileObjectNode->addr3=addr3;
					   pFileObjectNode->addr4=addr4;
					   pFileObjectNode->port=(USHORT)pIPAddress->sin_port;

					   pFileObjectNode->SET=TRUE;
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_SEND");
				   }
				   //发送信息
			       DebugPrintMsg(&event);
			   }
		       break;
		  
		   case TDI_ACCEPT:

			   pFileObjectNode=TDIH_GetFileObjectFromList(IrpStack->FileObject);
               if(pFileObjectNode==NULL)
			   {
				   DbgPrint("THERE IS SOMETHINT WRONG IN TDI_ACCEPT: THERE IS NO RECORD");
			   }
			   else
			   {
                   if(pFileObjectNode->SET==FALSE)
				   {
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_ACCEPT");
				   }
				   //发送信息
			       DebugPrintMsg(&event);
			   }
		       break;

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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_ACTION");
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_ASSOCIATE_ADDRESS");
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_DISASSOCIATE_ADDRESS");
				   }
				   //发送信息
			       DebugPrintMsg(&event);
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_DISCONNECT");
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_LISTEN");
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_QUERY_INFORMATION");
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_RECEIVE");
				   }
				   //发送信息
			       DebugPrintMsg(&event);
			   }
		       break;

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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_RECEIVE_DATAGRAM");
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
                       strcpy(event.addr1,"*");
					   strcpy(event.addr2,"*");
					   strcpy(event.addr3,"*");
					   strcpy(event.addr4,"*");
					   strcpy(event.port,"*");					   
				   }
				   else
				   {
					   //IP地址
					   sprintf(event.addr1, "%d" , pFileObjectNode->addr1);
			           sprintf(event.addr2, "%d" , pFileObjectNode->addr2);
			           sprintf(event.addr3, "%d" , pFileObjectNode->addr3);
			           sprintf(event.addr4, "%d" , pFileObjectNode->addr4);
                       

					   //端口号
					   sprintf(event.port,"%d",pFileObjectNode->port);

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
				   else
				   {
					   strcpy(event.Operation,"OTHER---TDI_SEND_DATAGRAM");
				   }
				   //发送信息
			       DebugPrintMsg(&event);
			   }
		       break;

		   default:
			   break;
	   }
   default:
	   break;
   }

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

}

VOID
TDIH_Close(
   PTDIH_DeviceExtension   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpStack
   )
{	
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
	
}

VOID
TDIH_CleanUp(
   PTDIH_DeviceExtension   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpStack
   )
{
	
	//KIRQL            OldIrql;

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
            //KeAcquireSpinLock(&FileObjectLock, &OldIrql); 
			RemoveEntryList(&pFileObjectNode->ListEntry);
			//KeReleaseSpinLock(&FileObjectLock, OldIrql);

			ExFreePool(pFileObjectNode);
		}
	}
	
}

PFILEOBJECT_NODE
TDIH_GetFileObjectFromList(PFILE_OBJECT pFileObject)
{
    
	PFILEOBJECT_NODE pFileObjectNode;
	//PLIST_ENTRY pdLink ;
 
	//KIRQL            OldIrql;                                                                     
    //KeAcquireSpinLock(&PROTECTLOCK, &OldIrql);                                                   

	pFileObjectNode=(PFILEOBJECT_NODE)FileObjectList.Flink;

	while( !IsListEmpty( &FileObjectList)
		&& pFileObjectNode != (PFILEOBJECT_NODE)&FileObjectList)
	{	
		if( pFileObjectNode->pFileObject == pFileObject )
		{
			return( pFileObjectNode );
		}

		pFileObjectNode=(PFILEOBJECT_NODE)pFileObjectNode->ListEntry.Flink;
	}

    //KeReleaseSpinLock(&PROTECTLOCK, OldIrql);

    return( (PFILEOBJECT_NODE)NULL );
}

VOID
TDIH_DeleteAllFileObjectNodes()
{
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
}


NTSTATUS
TCPFilter_Attach(
	IN PDRIVER_OBJECT	DriverObject,
	IN PUNICODE_STRING	RegistryPath
)
{
	NTSTATUS                   status;
   UNICODE_STRING             uniNtNameString;
   PTDIH_DeviceExtension pTDIH_DeviceExtension;
   PDEVICE_OBJECT             pFilterDeviceObject = NULL;
   PDEVICE_OBJECT             pTargetDeviceObject = NULL;
   PFILE_OBJECT               pTargetFileObject = NULL;
   PDEVICE_OBJECT             pLowerDeviceObject = NULL;

   ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );

   RtlInitUnicodeString( &uniNtNameString, DD_TCP_DEVICE_NAME );

   ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
   status = IoGetDeviceObjectPointer(
               &uniNtNameString,
               FILE_READ_ATTRIBUTES,
               &pTargetFileObject,   
               &pTargetDeviceObject
               );

   if( !NT_SUCCESS(status) )
   {
      pTargetFileObject = NULL;
      pTargetDeviceObject = NULL;

      return( status );
   }

   ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );
   RtlInitUnicodeString( &uniNtNameString, TDIH_TCP_DEVICE_NAME );

   ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
   status = IoCreateDevice(
               DriverObject,
               sizeof( TDIH_DeviceExtension ),
               &uniNtNameString,
               pTargetDeviceObject->DeviceType,
               pTargetDeviceObject->Characteristics,
               FALSE,                
               &pFilterDeviceObject
               );

   if( !NT_SUCCESS(status) )
   {
      KdPrint(("PCATDIH: Couldn't create the TCP Filter Device Object\n"));

      ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
      ObDereferenceObject( pTargetFileObject );

      pTargetFileObject = NULL;
      pTargetDeviceObject = NULL;

      return( status );
   }

   pTDIH_DeviceExtension = (PTDIH_DeviceExtension )( pFilterDeviceObject->DeviceExtension );

   TCPFilter_InitDeviceExtension(
		IN	pTDIH_DeviceExtension,
		IN	pFilterDeviceObject,
		IN	pTargetDeviceObject,
		IN	pTargetFileObject,
		IN	pLowerDeviceObject
		);

   KeInitializeSpinLock(&(pTDIH_DeviceExtension->IoRequestsSpinLock));

   KeInitializeEvent(&(pTDIH_DeviceExtension->IoInProgressEvent), NotificationEvent, FALSE);

   pLowerDeviceObject = IoAttachDeviceToDeviceStack(
                           pFilterDeviceObject, 
                           pTargetDeviceObject  
                           );

   if( !pLowerDeviceObject )
   {
      KdPrint(("PCATDIH: Couldn't attach to TCP Device Object\n"));

      IoDeleteDevice( pFilterDeviceObject );

      pFilterDeviceObject = NULL;

      ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
      ObDereferenceObject( pTargetFileObject );

      pTargetFileObject = NULL;
      pTargetDeviceObject = NULL;

      return( status );
   }

   // Initialize the TargetDeviceObject field in the extension.
   pTDIH_DeviceExtension->TargetDeviceObject = pTargetDeviceObject;
   pTDIH_DeviceExtension->TargetFileObject = pTargetFileObject;

   pTDIH_DeviceExtension->LowerDeviceObject = pLowerDeviceObject;

   pTDIH_DeviceExtension->DeviceExtensionFlags |= TDIH_DEV_EXT_ATTACHED;

   if( pLowerDeviceObject != pTargetDeviceObject )
   {
      KdPrint(("PCATDIH: TCP Already Filtered!\n"));
   }

  
   pFilterDeviceObject->Flags |= pTargetDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);

   return status;
	
}

NTSTATUS
UDPFilter_Attach(
	IN PDRIVER_OBJECT	DriverObject,
	IN PUNICODE_STRING	RegistryPath
)
{
	 NTSTATUS                   status;
   UNICODE_STRING             uniNtNameString;
   PTDIH_DeviceExtension pTDIH_DeviceExtension;
   PDEVICE_OBJECT             pFilterDeviceObject = NULL;
   PDEVICE_OBJECT             pTargetDeviceObject = NULL;
   PFILE_OBJECT               pTargetFileObject = NULL;
   PDEVICE_OBJECT             pLowerDeviceObject = NULL;

   KdPrint(("PCATDIH: UDPFilter_Attach Entry...\n"));

 
   ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );
   RtlInitUnicodeString( &uniNtNameString, DD_UDP_DEVICE_NAME );

   ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
   status = IoGetDeviceObjectPointer(
               &uniNtNameString,
               FILE_READ_ATTRIBUTES,
               &pTargetFileObject,   // Call ObDereferenceObject Eventually...
               &pTargetDeviceObject
               );

   if( !NT_SUCCESS(status) )
   {
      KdPrint(("PCATDIH: Couldn't get the UDP Device Object\n"));

      pTargetFileObject = NULL;
      pTargetDeviceObject = NULL;

      return( status );
   }

  
   ASSERT( KeGetCurrentIrql() <= DISPATCH_LEVEL );
   RtlInitUnicodeString( &uniNtNameString, TDIH_UDP_DEVICE_NAME );

  
   ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
   status = IoCreateDevice(
               DriverObject,
               sizeof( TDIH_DeviceExtension ),
               &uniNtNameString,
               pTargetDeviceObject->DeviceType,
               pTargetDeviceObject->Characteristics,
               FALSE,                 // This isn't an exclusive device
               &pFilterDeviceObject
               );

   if( !NT_SUCCESS(status) )
   {
      KdPrint(("PCATDIH: Couldn't create the UDP Filter Device Object\n"));

//      ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
      ObDereferenceObject( pTargetFileObject );

      pTargetFileObject = NULL;
      pTargetDeviceObject = NULL;

      return( status );
   }

   
   pTDIH_DeviceExtension = (PTDIH_DeviceExtension )( pFilterDeviceObject->DeviceExtension );

   UDPFilter_InitDeviceExtension(
		IN	pTDIH_DeviceExtension,
		IN	pFilterDeviceObject,
		IN	pTargetDeviceObject,
		IN	pTargetFileObject,
		IN	pLowerDeviceObject
		);

   // Initialize the Executive spin lock for this device extension.
   KeInitializeSpinLock(&(pTDIH_DeviceExtension->IoRequestsSpinLock));

   // Initialize the event object used to denote I/O in progress.
   // When set, the event signals that no I/O is currently in progress.
   KeInitializeEvent(&(pTDIH_DeviceExtension->IoInProgressEvent), NotificationEvent, FALSE);

  
   pLowerDeviceObject = IoAttachDeviceToDeviceStack(
                           pFilterDeviceObject, // Source Device (Our Device)
                           pTargetDeviceObject  // Target Device
                           );

   if( !pLowerDeviceObject )
   {
      KdPrint(("PCATDIH: Couldn't attach to UDP Device Object\n"));

      IoDeleteDevice( pFilterDeviceObject );

      pFilterDeviceObject = NULL;

      ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
      ObDereferenceObject( pTargetFileObject );

      pTargetFileObject = NULL;
      pTargetDeviceObject = NULL;

      return( status );
   }

   // Initialize the TargetDeviceObject field in the extension.
   pTDIH_DeviceExtension->TargetDeviceObject = pTargetDeviceObject;
   pTDIH_DeviceExtension->TargetFileObject = pTargetFileObject;

   pTDIH_DeviceExtension->LowerDeviceObject = pLowerDeviceObject;

   pTDIH_DeviceExtension->DeviceExtensionFlags |= TDIH_DEV_EXT_ATTACHED;

   if( pLowerDeviceObject != pTargetDeviceObject )
   {
      KdPrint(("PCATDIH: UDP Already Filtered!\n"));
   }

   pFilterDeviceObject->Flags |= pTargetDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);

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
	PTDIH_DeviceExtension pTDIH_DeviceExtension;
   BOOLEAN		NoRequestsOutstanding = FALSE;

   pTDIH_DeviceExtension = (PTDIH_DeviceExtension )pDeviceObject->DeviceExtension;

   ASSERT( pTDIH_DeviceExtension );

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
            {
				 KeWaitForSingleObject(
                 (void *)(&(pTDIH_DeviceExtension->IoInProgressEvent)),
                 Executive, KernelMode, FALSE, NULL
                 );

			}
            else
            {
				break;
			}
		}

		if( pTDIH_DeviceExtension->DeviceExtensionFlags & TDIH_DEV_EXT_ATTACHED)
        {
            IoDetachDevice( pTDIH_DeviceExtension->TargetDeviceObject );

            pTDIH_DeviceExtension->DeviceExtensionFlags &= ~(TDIH_DEV_EXT_ATTACHED);
		}

	    pTDIH_DeviceExtension->NodeType = 0;
	    pTDIH_DeviceExtension->NodeSize = 0;

        if( pTDIH_DeviceExtension->TargetFileObject )
        {
           ObDereferenceObject( pTDIH_DeviceExtension->TargetFileObject );
        }

        pTDIH_DeviceExtension->TargetFileObject = NULL;

		IoDeleteDevice( pDeviceObject );

        KdPrint(("PCATDIH: TCPFilter_Detach Finished\n"));
	}
    except (EXCEPTION_EXECUTE_HANDLER)
    {
			
	}
  }
  finally
  {
   
  }

  DbgPrint("TCP FILTER HAS BEEN DETACHED SUCCESSFULLY");

  return;


}

VOID
UDPFilter_Detach(
	IN	PDEVICE_OBJECT pDeviceObject
)
{
	PTDIH_DeviceExtension pTDIH_DeviceExtension;
   BOOLEAN		NoRequestsOutstanding = FALSE;

   pTDIH_DeviceExtension = (PTDIH_DeviceExtension )pDeviceObject->DeviceExtension;

   ASSERT( pTDIH_DeviceExtension );

   try
   {
      try
      {
         // We will wait until all IRP-based I/O requests have been completed.

         while (TRUE)
         {
				// Check if there are requests outstanding
            UTIL_IsLargeIntegerZero(
               NoRequestsOutstanding,
               pTDIH_DeviceExtension->OutstandingIoRequests,
               &(pTDIH_DeviceExtension->IoRequestsSpinLock)
               );

			if( !NoRequestsOutstanding )
            {
					// Drop the resource and go to sleep.

					// Worst case, we will allow a few new I/O requests to slip in ...
					KeWaitForSingleObject(
                  (void *)(&(pTDIH_DeviceExtension->IoInProgressEvent)),
                  Executive, KernelMode, FALSE, NULL
                  );

			}
            else
            {
				break;
			}
		}

			// Detach if attached.
		if( pTDIH_DeviceExtension->DeviceExtensionFlags & TDIH_DEV_EXT_ATTACHED)
        {
            IoDetachDevice( pTDIH_DeviceExtension->TargetDeviceObject );

            pTDIH_DeviceExtension->DeviceExtensionFlags &= ~(TDIH_DEV_EXT_ATTACHED);
		}

			// Delete our device object. But first, take care of the device extension.
	    pTDIH_DeviceExtension->NodeType = 0;
	    pTDIH_DeviceExtension->NodeSize = 0;

        if( pTDIH_DeviceExtension->TargetFileObject )
        {
            ObDereferenceObject( pTDIH_DeviceExtension->TargetFileObject );
        }

        pTDIH_DeviceExtension->TargetFileObject = NULL;

		IoDeleteDevice( pDeviceObject );

        KdPrint(("PCATDIH: UDPFilter_Detach Finished\n"));
	}
    except (EXCEPTION_EXECUTE_HANDLER)
    {
		
	}

   }
   finally
   {
      
   }

   DbgPrint("UDP FILTER HAS BEEN DETACHED SUCCESSFULLY");

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
	
	if(!NT_SUCCESS(status))
	{
		return;
	}

	status=ObReferenceObjectByHandle(ThreadHandle,THREAD_ALL_ACCESS,NULL,KernelMode,&ThreadObjectPointer,NULL);
	if(NT_SUCCESS(status))
		ZwClose(ThreadHandle);

}

void DebugPrintClose()
{
	ExitNow=TRUE;
	KeSetEvent(&ThreadEvent,0,FALSE);
	KeWaitForSingleObject(&ThreadExiting,Executive,KernelMode,FALSE,NULL);
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
				//DBGPRINT(("WOKAOHAIMEILIANSHANG"));
				break;
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
	
	DbgPrint("NICE TO MEET YOU");

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

	DbgPrint("ALL THE EVENTS HAVE BEEN DETACHED SUCCESSFULLY");
}


USHORT ANSIstrlen( char* str)
{
	USHORT len = 0;
	for(;*str++!='\0';)
		len++;
	return len;
}
