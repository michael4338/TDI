//////////////////////////////////////////////////////////////////////////////
//	Copyright © 1998 Chris Cant, PHD Computer Consultants Ltd
//
//	DebugPrint WDM driver
//
//	Test drivers write to the device, \Device\PHDDebugPrint
//	DebugPrintMonitor Win32 program reads from this device,
//		using our device interface to the first device
/////////////////////////////////////////////////////////////////////////////
//	init.cpp:		Driver initialization code
/////////////////////////////////////////////////////////////////////////////
//	DriverEntry		Initialisation entry point
//	DbpUnload		Unload driver routine
/////////////////////////////////////////////////////////////////////////////
//	Version history
//	20-Oct-98	1.0.0	CC	creation
/////////////////////////////////////////////////////////////////////////////

#include "DebugPrint.h"

#pragma code_seg("INIT") // start PAGE section

/////////////////////////////////////////////////////////////////////////////
//	DriverEntry:
//
//	Description:
//		This function initializes the driver, and creates
//		any objects needed to process I/O requests.
//
//	Arguments:
//		Pointer to the Driver object
//		Registry path string for driver service key
//
//	Return Value:
//		This function returns STATUS_XXX

extern "C"
NTSTATUS DriverEntry(	IN PDRIVER_OBJECT DriverObject,
						IN PUNICODE_STRING RegistryPath)
{
	NTSTATUS status = STATUS_SUCCESS;

	// Export other driver entry points...
 	DriverObject->DriverExtension->AddDevice = DbpAddDevice;
 	DriverObject->DriverUnload = DbpUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = DbpCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DbpClose;

	DriverObject->MajorFunction[IRP_MJ_PNP] = DbpPnp;
	DriverObject->MajorFunction[IRP_MJ_POWER] = DbpPower;

	DriverObject->MajorFunction[IRP_MJ_READ] = DbpRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = DbpWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DbpDeviceControl;

	DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = DbpSystemControl;

	return status;
}
#pragma code_seg() // end INIT section

//////////////////////////////////////////////////////////////////////////////
//	DbpUnload
//
//	Description:
//		Unload the driver by removing any remaining objects, etc.
//
//	Arguments:
//		Pointer to the Driver object
//
//	Return Value:
//		None

#pragma code_seg("PAGE") // start PAGE section

VOID DbpUnload(IN PDRIVER_OBJECT DriverObject)
{
}


//////////////////////////////////////////////////////////////////////////////
#pragma code_seg() // end PAGE section
