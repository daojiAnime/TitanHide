// Probe another process PID for DebugPort / DebugObjectHandle (external view).
// cl /O2 /EHsc /Fe:antidebug_probe_remote.exe antidebug_probe_remote.cpp
#include <windows.h>
#include <stdio.h>

typedef NTSTATUS(NTAPI* pNtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("usage: %s <pid>\n", argv[0]);
        return 1;
    }
    DWORD pid = (DWORD)strtoul(argv[1], nullptr, 0);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if(!h)
    {
        printf("OpenProcess fail %lu\n", GetLastError());
        return 2;
    }
    auto NtQIP = (pNtQIP)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    ULONG_PTR port = 0, obj = 0;
    ULONG ret = 0;
    NTSTATUS s1 = NtQIP(h, 7, &port, sizeof(port), &ret);
    NTSTATUS s2 = NtQIP(h, 30, &obj, sizeof(obj), &ret);
    DWORD flags = 0;
    NTSTATUS s3 = NtQIP(h, 0x1f, &flags, sizeof(flags), &ret);
    printf("pid=%lu\n", pid);
    printf("ProcessDebugPort st=0x%08lX port=0x%p\n", (unsigned long)s1, (void*)port);
    printf("ProcessDebugObjectHandle st=0x%08lX h=0x%p\n", (unsigned long)s2, (void*)obj);
    printf("ProcessDebugFlags st=0x%08lX flags=%lu\n", (unsigned long)s3, flags);
    int det = 0;
    if(s1 == 0 && port)
        det++;
    if(s2 == 0 && obj)
        det++;
    // NoDebugInherit: 0 often means debugged when st==0
    if(s3 == 0 && flags == 0)
        det++;
    printf("detections=%d\n", det);
    if(obj)
        CloseHandle((HANDLE)obj);
    CloseHandle(h);
    return det;
}
