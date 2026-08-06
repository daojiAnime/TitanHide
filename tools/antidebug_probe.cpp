// One-shot anti-debug probe for TiDaoji live A/B (self-targeted Nt* checks).
// Exit code = number of detections (0 = clean).
// Optional: writes RESULT_PATH (default C:\TiDaoji_antidebug_probe.txt)
//
// cl /O2 /EHsc /Fe:antidebug_probe.exe antidebug_probe.cpp
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtClose)(HANDLE);
typedef NTSTATUS(NTAPI* pNtQueryObject)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtCreateDebugObject)(PHANDLE, ACCESS_MASK, PVOID, ULONG);
typedef NTSTATUS(NTAPI* pNtSystemDebugControl)(ULONG, PVOID, ULONG, PVOID, ULONG, PULONG);

#ifndef NT_SUCCESS
#define NT_SUCCESS(s) (((NTSTATUS)(s)) >= 0)
#endif

struct UNI
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
};

struct OBJ_TYPE_INFO
{
    UNI TypeName;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
};

struct OBJ_ATTR
{
    ULONG Length;
    HANDLE RootDirectory;
    UNI* ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
};

static pNtQueryInformationProcess NtQIP;
static pNtQuerySystemInformation NtQSI;
static pNtClose NtCloseFn;
static pNtQueryObject NtQO;
static pNtCreateDebugObject NtCDO;
static pNtSystemDebugControl NtSDC;

static int g_detect = 0;
static FILE* g_out = nullptr;

static void emit(const char* name, int detected)
{
    if(detected)
        g_detect++;
    printf("%s=%d\n", name, detected);
    if(g_out)
        fprintf(g_out, "%s=%d\n", name, detected);
}

static int check_debug_flags()
{
    DWORD noInherit = 0;
    NTSTATUS st = NtQIP(GetCurrentProcess(), 0x1f, &noInherit, sizeof(noInherit), nullptr);
    if(st != 0)
        return 0;
    return noInherit == FALSE ? 1 : 0; // FALSE NoDebugInherit often means being debugged
}

static int check_debug_port()
{
    ULONG_PTR port = 0;
    ULONG ret = 0;
    NTSTATUS st = NtQIP(GetCurrentProcess(), 7, &port, sizeof(port), &ret);
    if(st != 0)
        return 0;
    return port ? 1 : 0;
}

static int check_debug_object_handle()
{
    ULONG_PTR h = 0;
    ULONG ret = 0;
    NTSTATUS st = NtQIP(GetCurrentProcess(), 30, &h, sizeof(h), &ret);
    if(st == (NTSTATUS)0xC0000353L) // STATUS_PORT_NOT_SET
        return 0;
    if(st != 0)
        return 0;
    if(h)
    {
        CloseHandle((HANDLE)h);
        return 1;
    }
    return 0;
}

static int check_system_debugger()
{
    // SystemKernelDebuggerInformation = 0x23
    struct
    {
        BOOLEAN KernelDebuggerEnabled;
        BOOLEAN KernelDebuggerNotPresent;
    } info = {};
    ULONG ret = 0;
    NTSTATUS st = NtQSI(0x23, &info, sizeof(info), &ret);
    if(!NT_SUCCESS(st))
        return 0;
    return (info.KernelDebuggerEnabled && !info.KernelDebuggerNotPresent) ? 1 : 0;
}

static int check_peb()
{
    BOOL wow = FALSE;
#ifndef _WIN64
    IsWow64Process(GetCurrentProcess(), &wow);
#endif
#ifdef _WIN64
    auto peb = (BYTE*)__readgsqword(0x60);
#else
    auto peb = (BYTE*)__readfsdword(0x30);
#endif
    if(!peb)
        return 0;
    return peb[2] ? 1 : 0; // BeingDebugged
}

static int check_object_type()
{
    if(!NtCDO || !NtQO)
        return 0;
    OBJ_ATTR oa = {};
    oa.Length = sizeof(oa);
    HANDLE dbg = nullptr;
    NTSTATUS st = NtCDO(&dbg, 0x0008, &oa, 0);
    if(!NT_SUCCESS(st) || !dbg)
        return 0;
    ULONG need = 0;
    st = NtQO(dbg, 2 /*ObjectTypeInformation*/, nullptr, 0, &need);
    int det = 0;
    if(st != (NTSTATUS)0xC0000004L || need < sizeof(OBJ_TYPE_INFO))
        det = 1;
    else
    {
        auto* ti = (OBJ_TYPE_INFO*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, need);
        if(!ti)
            det = 1;
        else
        {
            st = NtQO(dbg, 2, ti, need, nullptr);
            if(!NT_SUCCESS(st) || ti->TotalNumberOfObjects == 0)
                det = 1;
            HeapFree(GetProcessHeap(), 0, ti);
        }
    }
    CloseHandle(dbg);
    return det;
}

int main(int argc, char** argv)
{
    const char* path = "C:\\TiDaoji_antidebug_probe.txt";
    bool hold = false;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            path = argv[++i];
        else if(strcmp(argv[i], "-hold") == 0)
            hold = true;
    }

    // Hold if -hold or C:\TiDaoji_probe_hold exists (for x64dbg init without args).
    // Resume: create C:\TiDaoji_probe_go.txt (or timeout 90s).
    if(!hold && GetFileAttributesA("C:\\TiDaoji_probe_hold") != INVALID_FILE_ATTRIBUTES)
        hold = true;
    if(hold)
    {
        printf("pid=%lu HOLD waiting for C:\\TiDaoji_probe_go.txt\n", GetCurrentProcessId());
        fflush(stdout);
        DeleteFileA("C:\\TiDaoji_probe_go.txt");
        for(int t = 0; t < 900; t++)
        {
            if(GetFileAttributesA("C:\\TiDaoji_probe_go.txt") != INVALID_FILE_ATTRIBUTES)
                break;
            Sleep(100);
        }
        DeleteFileA("C:\\TiDaoji_probe_go.txt");
    }

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    NtQIP = (pNtQueryInformationProcess)GetProcAddress(ntdll, "NtQueryInformationProcess");
    NtQSI = (pNtQuerySystemInformation)GetProcAddress(ntdll, "NtQuerySystemInformation");
    NtCloseFn = (pNtClose)GetProcAddress(ntdll, "NtClose");
    NtQO = (pNtQueryObject)GetProcAddress(ntdll, "NtQueryObject");
    NtCDO = (pNtCreateDebugObject)GetProcAddress(ntdll, "NtCreateDebugObject");
    NtSDC = (pNtSystemDebugControl)GetProcAddress(ntdll, "NtSystemDebugControl");

    g_out = fopen(path, "w");
    printf("pid=%lu\n", GetCurrentProcessId());
    if(g_out)
        fprintf(g_out, "pid=%lu\n", GetCurrentProcessId());

    emit("PEB_BeingDebugged", check_peb());
    emit("ProcessDebugFlags", check_debug_flags());
    emit("ProcessDebugPort", check_debug_port());
    emit("ProcessDebugObjectHandle", check_debug_object_handle());
    emit("SystemKernelDebugger", check_system_debugger());
    emit("NtQueryObject_DebugObjectType", check_object_type());

    printf("detections=%d\n", g_detect);
    if(g_out)
    {
        fprintf(g_out, "detections=%d\n", g_detect);
        fclose(g_out);
    }
    return g_detect;
}
