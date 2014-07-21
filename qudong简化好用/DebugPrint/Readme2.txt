Copyright © 1998,1999 Chris Cant, PHD Computer Consultants Ltd

DebugPrint is a full featured software suite to help programmers debug their drivers
by inserting formatted print trace statements in their code.

The SYS subdirectory contains the driver source.
Use the Control Panel Add New Hardware Wizard to install a DebugPrint device
using the DebugPrint.INF file in SYS.

The EXE subdirectory contains the DebugPrint Monitor application.
This lets you see the trace messages coming from test drivers.
Run EXE\Release\DebugPrintMonitor to run the Monitor application.

Each driver under test must include the DebugPrint test driver code in DebugPrint.*.
By default, the DebugPrint output is only produced in the checked build.

Read index.html for full instructions.
