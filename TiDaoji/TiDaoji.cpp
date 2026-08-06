#include "hooks.h"
#include "undocumented.h"
#include "ssdt.h"
#include "hider.h"
#include "log.h"
#include "ntdll.h"
#include "threadhidefromdbg.h"
// PR3: production hide is InfinityHook (k_hook). SSDT write path is unused.
#include "infinity_hook/hook.h"
#include "TiDaoji.h"

// Undocumented but used by manual-map loaders (L2/L3): create a real DRIVER_OBJECT
// when mapper calls DriverEntry with DriverObject == NULL.
extern "C" NTKERNELAPI NTSTATUS NTAPI IoCreateDriver(
    _In_opt_ PUNICODE_STRING DriverName,
    _In_ PDRIVER_INITIALIZE InitializationFunction
);

static UNICODE_STRING DeviceName;
static wchar_t DeviceNameBuffer[256];
static UNICODE_STRING Win32Device;
static wchar_t Win32DeviceBuffer[256];

static PDRIVER_OBJECT g_DriverObject = nullptr;
static PDEVICE_OBJECT g_DeviceObject = nullptr;
static volatile LONG g_TornDown = 0;
static BOOLEAN g_ViaIoCreateDriver = FALSE;
static BOOLEAN g_SymlinkOk = FALSE;

// After Cleanup, CPUs may still run TiDaojiSyscallCallback/HookNt*.
// No per-syscall refcount; fixed drain shrinks UAF window (not formal).
// Default 5s; override with -DTIDAOJI_UNLOAD_DRAIN_MS=10000 for research unload.
#ifndef TIDAOJI_UNLOAD_DRAIN_MS
#define TIDAOJI_UNLOAD_DRAIN_MS 5000
#endif

static void DrainInflight()
{
    LARGE_INTEGER delay;
    delay.QuadPart = -(LONGLONG)TIDAOJI_UNLOAD_DRAIN_MS * 10000LL;
    Log("[TIDAOJI] Unload drain %d ms\r\n", TIDAOJI_UNLOAD_DRAIN_MS);
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

// Shared teardown for SCM Unload and SoftUnload (L2/L3).
static void FullTeardown()
{
    if(InterlockedCompareExchange(&g_TornDown, 1, 0) != 0)
    {
        Log("[TIDAOJI] FullTeardown already done\r\n");
        return;
    }

    Log("[TIDAOJI] FullTeardown begin\r\n");
    Hooks::Deinitialize(); // k_hook::Cleanup()
    DrainInflight();

    if(g_SymlinkOk)
    {
        IoDeleteSymbolicLink(&Win32Device);
        g_SymlinkOk = FALSE;
    }
    if(g_DeviceObject)
    {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = nullptr;
    }
    NTDLL::Deinitialize();
    Log("[TIDAOJI] FullTeardown done\r\n");
}

static void DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    FullTeardown();
}

static NTSTATUS DriverCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS DriverDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS DriverWrite(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS RetStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);
    if(pIoStackIrp)
    {
        PVOID pInBuffer = (PVOID)Irp->AssociatedIrp.SystemBuffer;
        ULONG len = pIoStackIrp->Parameters.Write.Length;
        if(pInBuffer && len >= sizeof(HIDE_INFO))
        {
            HIDE_INFO* hi = (HIDE_INFO*)pInBuffer;
            if(hi->Command == SoftUnload)
            {
                Log("[TIDAOJI] SoftUnload requested (L2/L3 path)\r\n");
                FullTeardown();
            }
            else if(Hider::ProcessData(pInBuffer, len))
                Log("[TIDAOJI] HiderProcessData OK!\r\n");
            else
            {
                Log("[TIDAOJI] HiderProcessData failed...\r\n");
                RetStatus = STATUS_UNSUCCESSFUL;
            }
        }
        else if(pInBuffer)
        {
            if(Hider::ProcessData(pInBuffer, len))
                Log("[TIDAOJI] HiderProcessData OK!\r\n");
            else
            {
                Log("[TIDAOJI] HiderProcessData failed...\r\n");
                RetStatus = STATUS_UNSUCCESSFUL;
            }
        }
    }
    else
    {
        Log("[TIDAOJI] Invalid IRP stack pointer...\r\n");
        RetStatus = STATUS_UNSUCCESSFUL;
    }
    Irp->IoStatus.Status = RetStatus;
    Irp->IoStatus.Information = NT_SUCCESS(RetStatus) && pIoStackIrp
                                ? pIoStackIrp->Parameters.Write.Length
                                : 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return RetStatus;
}

static void TeardownDevicePartial(IN PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    if(g_SymlinkOk)
    {
        IoDeleteSymbolicLink(&Win32Device);
        g_SymlinkOk = FALSE;
    }
    if(g_DeviceObject)
    {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = nullptr;
    }
}

// Real init once we have a DRIVER_OBJECT (SCM or IoCreateDriver).
static NTSTATUS DriverInitialize(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
    g_DriverObject = DriverObject;
    g_TornDown = 0;

    RtlInitEmptyUnicodeString(&DeviceName, DeviceNameBuffer, sizeof(DeviceNameBuffer));
    RtlAppendUnicodeToString(&DeviceName, L"\\Device\\");
    RtlInitEmptyUnicodeString(&Win32Device, Win32DeviceBuffer, sizeof(Win32DeviceBuffer));
    RtlAppendUnicodeToString(&Win32Device, L"\\DosDevices\\");

    UNICODE_STRING DriverName = {};
    if(RegistryPath != NULL && RegistryPath->Buffer != NULL)
    {
        const USHORT CharacterCount = (USHORT)(RegistryPath->Length / sizeof(WCHAR));
        for(USHORT i = 0; i < CharacterCount; i++)
        {
            USHORT index = CharacterCount - i - 1;
            if(RegistryPath->Buffer[index] == L'\\')
            {
                index++;
                DriverName.Buffer = RegistryPath->Buffer + index;
                DriverName.Length = (USHORT)(RegistryPath->Length - index * sizeof(WCHAR));
                DriverName.MaximumLength = DriverName.Length;
                break;
            }
        }
    }

    if(DriverName.Length == 0)
        RtlInitUnicodeString(&DriverName, L"TiDaoji");

    RtlAppendUnicodeStringToString(&DeviceName, &DriverName);
    RtlAppendUnicodeStringToString(&Win32Device, &DriverName);
    InitLog(&DriverName);
    Log("[TIDAOJI] DriverName set (manual-map safe fallback TiDaoji)\r\n");
    if(g_ViaIoCreateDriver)
        Log("[TIDAOJI] load path: IoCreateDriver (L2/L3 manual-map style)\r\n");
    else
        Log("[TIDAOJI] load path: SCM / normal DriverObject (L1)\r\n");

    PDEVICE_OBJECT DeviceObject = NULL;
    NTSTATUS status;

    DriverObject->DriverUnload = DriverUnload;
    for(unsigned int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = DriverDefaultHandler;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = DriverWrite;

    if(!NT_SUCCESS(NTDLL::Initialize()))
    {
        Log("[TIDAOJI] Ntdll::Initialize() failed...\r\n");
        return STATUS_UNSUCCESSFUL;
    }

    if(!Undocumented::UndocumentedInit())
    {
        Log("[TIDAOJI] UndocumentedInit() failed...\r\n");
        NTDLL::Deinitialize();
        return STATUS_UNSUCCESSFUL;
    }
    Log("[TIDAOJI] UndocumentedInit() was successful!\r\n");

    status = FindCrossThreadFlagsOffset(&CrossThreadFlagsOffset);
    if(!NT_SUCCESS(status))
    {
        Log("[TIDAOJI] FindCrossThreadFlagsOffset() failed\r\n");
        NTDLL::Deinitialize();
        return status;
    }

    status = IoCreateDevice(DriverObject,
                            0,
                            &DeviceName,
                            FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &DeviceObject);
    if(!NT_SUCCESS(status) || !DeviceObject)
    {
        Log("[TIDAOJI] IoCreateDevice Error...\r\n");
        NTDLL::Deinitialize();
        return !NT_SUCCESS(status) ? status : STATUS_UNEXPECTED_IO_ERROR;
    }
    g_DeviceObject = DeviceObject;
    Log("[TIDAOJI] Device created successfully!\r\n");

    DeviceObject->Flags |= DO_BUFFERED_IO;
    DeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
    status = IoCreateSymbolicLink(&Win32Device, &DeviceName);
    if(!NT_SUCCESS(status))
    {
        Log("[TIDAOJI] IoCreateSymbolicLink Error...\r\n");
        IoDeleteDevice(DeviceObject);
        g_DeviceObject = nullptr;
        NTDLL::Deinitialize();
        return status;
    }
    g_SymlinkOk = TRUE;
    Log("[TIDAOJI] Symbolic link created!\r\n");

    const int hooked = Hooks::Initialize();
    if(hooked <= 0)
    {
        Log("[TIDAOJI] Hooks::Initialize failed - abort load\r\n");
        k_hook::Cleanup();
        TeardownDevicePartial(DriverObject);
        NTDLL::Deinitialize();
        return STATUS_UNSUCCESSFUL;
    }
    Log("[TIDAOJI] InfinityHook hide armed (functions hooked)\r\n");
    Log("[TIDAOJI] SoftUnload supported via HIDE_INFO.Command=SoftUnload\r\n");

    return STATUS_SUCCESS;
}

// L1: SCM passes real DriverObject.
// L2/L3 mappers often pass NULL DriverObject / NULL RegistryPath -> IoCreateDriver.
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
    if(DriverObject == NULL)
    {
        g_ViaIoCreateDriver = TRUE;
        UNICODE_STRING drvName;
        RtlInitUnicodeString(&drvName, L"\\Driver\\TiDaoji");
        // IoCreateDriver invokes DriverInitialize with a real DRIVER_OBJECT.
        return IoCreateDriver(&drvName, DriverInitialize);
    }

    g_ViaIoCreateDriver = FALSE;
    return DriverInitialize(DriverObject, RegistryPath);
}
