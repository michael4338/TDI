///////////////////////////////////////////////////////////////////////
// Copyright (c) 2001-2002
// XStudio Technology All Right Reserved.
// Author: Tony Zhu
// 2001-7-10 Create
// summary:
//			This Program to demo the Intermediate TDI Driver.
///////////////////////////////////////////////////////////////////////
//Driver's Function
//——————————————————————————————————————
// DD_TCP_DEVICE_NAME	: 系统设备名称，我们将挂接自己设备到这个设备之上
// TDIH_TCP_DEVICE_NAME : 自己的设备名称
// TDIH_DEV_EXT_ATTACHED: 自己设备的扩展标志
//

#define DD_TCP_DEVICE_NAME      L"\\Device\\Tcp"
#define DD_UDP_DEVICE_NAME      L"\\Device\\Udp"
#define TDIH_TCP_DEVICE_NAME    L"\\Device\\LLTTcpFilter"
#define TDIH_UDP_DEVICE_NAME    L"\\Device\\LLTUdpFilter"
#define	TDIH_DEV_EXT_ATTACHED	(0x00000001)
/**************************************************************************
	每一个结构必须有一个唯一的“node type”或者一个联合签名
**************************************************************************/
#define   TDIH_NODE_TYPE_TCP_FILTER_DEVICE      (0xfdecba12)
#define   TDIH_NODE_TYPE_UDP_FILTER_DEVICE      (0xfdecba13)

/*
	输出调试信息
*/
#define DBGPRINT(Fmt)												\
{																	\
	DbgPrint(" ***FilterTdiDriver.sys*** ");						\
	DbgPrint (Fmt);													\
}

/*
	进行64位数值的处理，请参阅DDK帮助文档关于RtlLargeIntegerEqualToZero的解释
*/
#define  UTIL_IncrementLargeInteger(LargeIntegerOp, ULongIncrement, pSpinLock)   {         \
   KIRQL   OldIrql;                                                                     \
   KeAcquireSpinLock(pSpinLock, &OldIrql);                                                   \
   LargeIntegerOp+=ULongIncrement;   \
   KeReleaseSpinLock(pSpinLock, OldIrql);                                                   \
}

// the following macro allows us to decrement a large integer value atomically.
// we expect an unsigned long to be supplied as the decrement value.
// a spin lock should be passed-in to synchronize operations
#define UTIL_DecrementLargeInteger(LargeIntegerOp, ULongIncrement, pSpinLock)   {         \
   KIRQL            OldIrql;                                                                     \
   KeAcquireSpinLock(pSpinLock, &OldIrql);                                                   \
   LargeIntegerOp-=ULongIncrement;   \
   KeReleaseSpinLock(pSpinLock, OldIrql);                                                   \
}

// the following macro allows us to check if the large integer value is zero,
// atomically. Note that I have added (for convenience) a check to ensure that
// the value is non-negative.
#define UTIL_IsLargeIntegerZero(ReturnValue, LargeIntegerOp, pSpinLock)   {               \
   KIRQL            OldIrql;                                                                     \
   KeAcquireSpinLock(pSpinLock, &OldIrql);                                                   \
   ReturnValue = (LargeIntegerOp==0);                                 \
   KeReleaseSpinLock(pSpinLock, OldIrql);                                                   \
}



//——————————————————————————————————————
// 用来保存驱动程序相关信息的自定义结构类型，这个结构类型可以绑定到
// DEVICE_OBJECT对象的DeviceExtension成员变量之上，随着DEVICE_OBJECT对象在
// 不同的函数之间传递
//
typedef struct _TDIH_DeviceExtension
{
	
	ULONG			NodeType;				// 标识这个结构	
	ULONG			NodeSize;				// 这个结构的大小
	PDEVICE_OBJECT	pFilterDeviceObject;	// 过滤设备对象
	KSPIN_LOCK		IoRequestsSpinLock;		// 同时调用时的保护锁
	KEVENT			IoInProgressEvent;		// 进程间同步处理
	ULONG			DeviceExtensionFlags;	// 设备标志
	PDEVICE_OBJECT	TargetDeviceObject;		// 绑定的设备对象
	PFILE_OBJECT    TargetFileObject;		// 绑定设备的文件对象
	PDEVICE_OBJECT  LowerDeviceObject;		// 绑定前底层设备对象
	ULONG           OutstandingIoRequests;	

}	TDIH_DeviceExtension, *PTDIH_DeviceExtension;


///////////////////////////////////////////////////////////////////////

NTSTATUS
DriverEntry(
	IN	PDRIVER_OBJECT		DriverObject,
	IN	PUNICODE_STRING		RegistryPath
);

VOID 
TDIH_Unload(
	IN PDRIVER_OBJECT		DriverObject
);

NTSTATUS
TDIH_DefaultDispatch(
    IN PDEVICE_OBJECT		DeviceObject,
    IN PIRP					Irp
);

NTSTATUS
DefaultDispatchCompletion(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PIRP			Irp,
	IN	PVOID			Context
);
NTSTATUS
TDIH_DispatchInternalDeviceControl(
	IN PDEVICE_OBJECT		DeviceObject,
    IN PIRP					Irp
);

NTSTATUS
TCPFilter_Attach(
	IN PDRIVER_OBJECT	DriverObject,
	IN PUNICODE_STRING	RegistryPath
);

NTSTATUS
UDPFilter_Attach(
	IN PDRIVER_OBJECT	DriverObject,
	IN PUNICODE_STRING	RegistryPath
);

NTSTATUS
TCPFilter_InitDeviceExtension(
	IN	PTDIH_DeviceExtension	pTDIH_DeviceExtension,
	IN	PDEVICE_OBJECT			pFilterDeviceObject,
	IN	PDEVICE_OBJECT			pTargetDeviceObject,
	IN	PFILE_OBJECT			pTargetFileObject,
	IN	PDEVICE_OBJECT			pLowerDeviceObject
);

NTSTATUS
UDPFilter_InitDeviceExtension(
	IN	PTDIH_DeviceExtension	pTDIH_DeviceExtension,
	IN	PDEVICE_OBJECT			pFilterDeviceObject,
	IN	PDEVICE_OBJECT			pTargetDeviceObject,
	IN	PFILE_OBJECT			pTargetFileObject,
	IN	PDEVICE_OBJECT			pLowerDeviceObject
);

VOID
TCPFilter_Detach(
   IN	PDEVICE_OBJECT pDeviceObject
);

VOID
UDPFilter_Detach(
   IN	PDEVICE_OBJECT pDeviceObject
);

VOID TDIH_Create(PTDIH_DeviceExtension pTDIH_DeviceExtension, 
				 PIRP Irp, 
				 PIO_STACK_LOCATION IrpStack
);
VOID TDIH_Close(PTDIH_DeviceExtension pTDIH_DeviceExtension,
				PIRP Irp, 
				PIO_STACK_LOCATION IrpStack
);
VOID TDIH_CleanUp(PTDIH_DeviceExtension pTDIH_DeviceExtension, 
				  PIRP Irp, 
				  PIO_STACK_LOCATION IrpStack
);
VOID
TDIH_DeleteAllFileObjectNodes();

NTSTATUS LLT_QueryAddressInfo(PFILE_OBJECT pFileObject,PVOID pInfoBuffer,PULONG pInfoBufferSize);

VOID
LLT_UnlockAndFreeMdl(
   PMDL pMdl
);
PMDL
LLT_AllocateAndProbeMdl(
   PVOID VirtualAddress,
   ULONG Length,
   BOOLEAN SecondaryBuffer,
   BOOLEAN ChargeQuota,
   PIRP Irp OPTIONAL
);
NTSTATUS
LLT_MakeSimpleTdiRequest(
   IN PDEVICE_OBJECT pDeviceObject,
   IN PIRP pIrp
);
NTSTATUS
LLT_SimpleTdiRequestComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
);
USHORT 
LLT_htons( USHORT hostshort );

//文件对象链表定义
typedef struct _FILEOBJECT_NODE
{
	LIST_ENTRY ListEntry;
	PFILE_OBJECT pFileObject;
	//PTDI_ADDRESS_IP IP;
	BOOLEAN SET;
	//USHORT PORT;
	int addr1;
	int addr2;
	int addr3;
	int addr4;
	USHORT LocalPort;
	USHORT RemotePort;
}FILEOBJECT_NODE,*PFILEOBJECT_NODE;

KSPIN_LOCK FileObjectLock;
LIST_ENTRY FileObjectList;

KSPIN_LOCK PROTECTLOCK;

PFILEOBJECT_NODE
TDIH_GetFileObjectFromList(PFILE_OBJECT pFileObject);

//这是与应用程序通信所加的方法，以传递信息

//事件定义
typedef struct _EVENT
{
	char ProcessID[10];
	char ProcessName[20];
    char Operation[50];
	char addr1[5];
	char addr2[5];
	char addr3[5];
	char addr4[5];
	char LocalPort[8];
	char RemotePort[8];
	char SuccOrFail[15];
}EVENT,*PEVENT;

void DebugPrintMsg(PEVENT event);
void DebugPrintInit(char* DriverName);
void DebugPrintClose();
void DebugPrintSystemThread(IN PVOID Context);
void ClearEvents();
USHORT ANSIstrlen( char* str);
//这是与应用程序通信所加的结构与变量，以传递信息

//事件链表定义
typedef struct _DEBUGPRINT_EVENT
{
	LIST_ENTRY ListEntry;
	ULONG Len;
	UCHAR EventData[1];
}DEBUGPRINT_EVENT,*PDEBUGPRINT_EVENT;

KSPIN_LOCK EventListLock;
LIST_ENTRY EventList;

//一些全局事件对象和变量
KEVENT ThreadEvent;
KEVENT ThreadExiting;
PVOID ThreadObjectPointer;
BOOLEAN ExitNow,DebugPrintStarted;


