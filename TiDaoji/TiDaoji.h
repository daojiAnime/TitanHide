#ifndef _TIDAOJI_H
#define _TIDAOJI_H

#define BIT(x) (1<<(x-1))

//enums
enum HIDE_TYPE
{
    HideProcessDebugFlags = BIT(1), //NtQueryInformationProcess
    HideProcessDebugPort = BIT(2), //NtQueryInformationProcess
    HideProcessDebugObjectHandle = BIT(3), //NtQueryInformationProcess
    HideDebugObject = BIT(4), //NtQueryObject
    HideSystemDebuggerInformation = BIT(5), //NtQuerySystemInformation
    HideNtClose = BIT(6), //NtClose
    HideThreadHideFromDebugger = BIT(7), //NtSetInformationThread
    HideNtGetContextThread = BIT(8), //NtGetContextThread
    HideNtSetContextThread = BIT(9), //NtSetContextThread
    HideNtSystemDebugControl = BIT(10), //NtSystemDebugControl
    // NtQuerySystemInformation(SystemFirmwareTableInformation) VM string scrub (lityrgia)
    HideNtSystemVMInformation = BIT(11)
};

enum HIDE_COMMAND
{
    HidePid,     // Hide a process
    UnhidePid,   // Unhide a process
    UnhideAll,   // Unhide everything
    // L2/L3 manual-map / no-SCM: tear down IH + device without sc stop.
    // Safe to ignore on old clients; new plugins may send SoftUnload.
    SoftUnload
};

//structures
struct HIDE_INFO
{
    HIDE_COMMAND Command;
    ULONG Type;
    ULONG Pid;
};

// Loader profile tags (usermode scripts / docs only; not sent to kernel)
// L1 = DSE window + sc start
// L2 = external kdmapper-class manual map
// L3 = alternate BYOVD / multi-provider map (optional)

#endif // _TIDAOJI_H
