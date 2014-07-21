; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CDebugPrint_MonitorDlg
LastTemplate=CListView
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "DebugPrint_Monitor.h"

ClassCount=4
Class1=CDebugPrint_MonitorApp
Class2=CDebugPrint_MonitorDlg
Class3=CAboutDlg

ResourceCount=3
Resource1=IDD_ABOUTBOX
Resource2=IDR_MAINFRAME
Class4=CEventListView
Resource3=IDD_DEBUGPRINT_MONITOR_DIALOG

[CLS:CDebugPrint_MonitorApp]
Type=0
HeaderFile=DebugPrint_Monitor.h
ImplementationFile=DebugPrint_Monitor.cpp
Filter=N

[CLS:CDebugPrint_MonitorDlg]
Type=0
HeaderFile=DebugPrint_MonitorDlg.h
ImplementationFile=DebugPrint_MonitorDlg.cpp
Filter=D
LastObject=CDebugPrint_MonitorDlg
BaseClass=CDialog
VirtualFilter=dWC

[CLS:CAboutDlg]
Type=0
HeaderFile=DebugPrint_MonitorDlg.h
ImplementationFile=DebugPrint_MonitorDlg.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[DLG:IDD_DEBUGPRINT_MONITOR_DIALOG]
Type=1
Class=CDebugPrint_MonitorDlg
ControlCount=9
Control1=IDC_BUTTON_INSTALL_DBG,button,1342242816
Control2=IDC_BUTTON_UNINSTALL_DBG,button,1342242816
Control3=IDC_BUTTON_INSTALL_TDI,button,1342242816
Control4=IDC_BUTTON_UNINSTALL_TDI,button,1476460544
Control5=IDC_BUTTON_PROCESS,button,1342242816
Control6=IDC_BUTTON_RECORD,button,1342242816
Control7=IDC_LIST,SysListView32,1350633473
Control8=IDC_LIST_RECORD,SysListView32,1350633473
Control9=IDC_STATIC,static,1476399111

[CLS:CEventListView]
Type=0
HeaderFile=EventListView.h
ImplementationFile=EventListView.cpp
BaseClass=CListView
Filter=C
VirtualFilter=VWC
LastObject=CEventListView

