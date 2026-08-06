#include "hooks.h"
#include "undocumented.h"
#include "ssdt.h"
#include "hider.h"
#include "log.h"
#include "ntdll.h"
#include "threadhidefromdbg.h"
// PR3: production hide is InfinityHook (k_hook). SSDT write path is unused.
#include "infinity_hook/hook.h"

static UNICODE_STRING DeviceName;
static wchar_t DeviceNameBuffer[256];
static UNICODE_STRING Win32Device;
static wchar_t Win32DeviceBuffer[256];

static void DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
    // Order: stop IH first → drain inflight syscalls → delete device → NTDLL
    Hooks::Deinitialize(); // k_hook::Cleanup()

    {
        LARGE_INTEGER delay;
        delay.QuadPart = -1LL * 1000 * 1000 * 10; // 1s relative
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    IoDeleteSymbolicLink(&Win32Device);
    IoDeleteDevice(DriverObject->DeviceObject);
    NTDLL::Deinitialize();
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
        if(pInBuffer)
        {
            if(Hider::ProcessData(pInBuffer, pIoStackIrp->Parameters.Write.Length))
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
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return RetStatus;
}

static void TeardownDevice(IN PDRIVER_OBJECT DriverObject)
{
    IoDeleteSymbolicLink(&Win32Device);
    if(DriverObject->DeviceObject)
        IoDeleteDevice(DriverObject->DeviceObject);
}

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
    // Initialize name buffers
    RtlInitEmptyUnicodeString(&DeviceName, DeviceNameBuffer, sizeof(DeviceNameBuffer));
    RtlAppendUnicodeToString(&DeviceName, L"\\Device\\");
    RtlInitEmptyUnicodeString(&Win32Device, Win32DeviceBuffer, sizeof(Win32DeviceBuffer));
    RtlAppendUnicodeToString(&Win32Device, L"\\DosDevices\\");

    // Derive the device name and symbolic link from the registry path
    UNICODE_STRING DriverName = {};
    if(RegistryPath != NULL && RegistryPath->Buffer != NULL)
    {
        const USHORT CharacterCount = (USHORT)(RegistryPath->Length / sizeof(WCHAR));
        for(USHORT i = 0; i < CharacterCount; i++)
        {
            USHORT index = CharacterCount - i - 1;
            if(RegistryPath->Buffer[index] == L'\\')
            {
                index++; // skip the backslash
                DriverName.Buffer = RegistryPath->Buffer + index;
                DriverName.Length = (USHORT)(RegistryPath->Length - index * sizeof(WCHAR));
                DriverName.MaximumLength = DriverName.Length;
                break;
            }
        }
    }

    // Fall back to default driver name
    if(DriverName.Length == 0)
    {
        RtlInitUnicodeString(&DriverName, L"TiDaoji");
    }

    // Use the driver name
    RtlAppendUnicodeStringToString(&DeviceName, &DriverName);
    RtlAppendUnicodeStringToString(&Win32Device, &DriverName);
    InitLog(&DriverName);
    Log("[TIDAOJI] DriverName: %.*ws\r\n", DriverName.Length / sizeof(WCHAR), DriverName.Buffer);

    PDEVICE_OBJECT DeviceObject = NULL;
    NTSTATUS status;

    //set callback functions
    DriverObject->DriverUnload = DriverUnload;
    for(unsigned int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = DriverDefaultHandler;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = DriverWrite;

    //read ntdll.dll from disk so we can use it for exports
    if(!NT_SUCCESS(NTDLL::Initialize()))
    {
        Log("[TIDAOJI] Ntdll::Initialize() failed...\r\n");
        return STATUS_UNSUCCESSFUL;
    }

    //initialize undocumented APIs
    if(!Undocumented::UndocumentedInit())
    {
        Log("[TIDAOJI] UndocumentedInit() failed...\r\n");
        NTDLL::Deinitialize();
        return STATUS_UNSUCCESSFUL;
    }
    Log("[TIDAOJI] UndocumentedInit() was successful!\r\n");

    //find the offset of CrossThreadFlags in ETHREAD
    status = FindCrossThreadFlagsOffset(&CrossThreadFlagsOffset);
    if(!NT_SUCCESS(status))
    {
        Log("[TIDAOJI] FindCrossThreadFlagsOffset() failed: 0x%lX\r\n", status);
        NTDLL::Deinitialize();
        return status;
    }

    //create io device
    status = IoCreateDevice(DriverObject,
                            0,
                            &DeviceName,
                            FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &DeviceObject);
    if(!NT_SUCCESS(status))
    {
        Log("[TIDAOJI] IoCreateDevice Error...\r\n");
        NTDLL::Deinitialize();
        return status;
    }
    if(!DeviceObject)
    {
        Log("[TIDAOJI] Unexpected I/O Error...\r\n");
        NTDLL::Deinitialize();
        return STATUS_UNEXPECTED_IO_ERROR;
    }
    Log("[TIDAOJI] Device %.*ws created successfully!\r\n", DeviceName.Length / sizeof(WCHAR), DeviceName.Buffer);

    //create symbolic link
    DeviceObject->Flags |= DO_BUFFERED_IO;
    DeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
    status = IoCreateSymbolicLink(&Win32Device, &DeviceName);
    if(!NT_SUCCESS(status))
    {
        Log("[TIDAOJI] IoCreateSymbolicLink Error...\r\n");
        IoDeleteDevice(DeviceObject);
        NTDLL::Deinitialize();
        return status;
    }
    Log("[TIDAOJI] Symbolic link %.*ws->%.*ws created!\r\n", Win32Device.Length / sizeof(WCHAR), Win32Device.Buffer, DeviceName.Length / sizeof(WCHAR), DeviceName.Buffer);

    // PR3: InfinityHook hide — fail hard on 0 so CKCL/layer B never half-armed
    const int hooked = Hooks::Initialize();
    if(hooked <= 0)
    {
        Log("[TIDAOJI] Hooks::Initialize failed (%d) — abort load\r\n", hooked);
        k_hook::Cleanup(); // idempotent; covers Start-fail residual
        TeardownDevice(DriverObject);
        NTDLL::Deinitialize();
        return STATUS_UNSUCCESSFUL;
    }
    Log("[TIDAOJI] Hooks::Initialize armed %d functions (InfinityHook)\r\n", hooked);

    return STATUS_SUCCESS;
}
