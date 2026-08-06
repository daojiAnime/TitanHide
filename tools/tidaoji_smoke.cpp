// Minimal live smoke for TiDaoji.sys: open device, HidePid/UnhidePid self.
// Build (win-master): cl /O2 /EHsc /Fe:tidaoji_smoke.exe tidaoji_smoke.cpp
// Run elevated after sc start TiDaoji.
#include <windows.h>
#include <stdio.h>
#include "../TiDaoji/TiDaoji.h"

static int usage(const char* argv0)
{
    printf("usage: %s [driverName] [pid]\n", argv0);
    printf("  default driverName=TiDaoji  pid=current\n");
    return 1;
}

int main(int argc, char** argv)
{
    const char* name = (argc >= 2) ? argv[1] : "TiDaoji";
    DWORD pid = (argc >= 3) ? (DWORD)strtoul(argv[2], nullptr, 0) : GetCurrentProcessId();

    char path[160];
    sprintf_s(path, "\\\\.\\%s", name);
    printf("[*] open %s  pid=%lu (0x%lX)\n", path, pid, pid);

    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if(h == INVALID_HANDLE_VALUE)
    {
        printf("[!] CreateFile failed Win32=%lu\n", GetLastError());
        return 2;
    }
    printf("[+] device open OK\n");

    HIDE_INFO hi = {};
    hi.Command = HidePid;
    hi.Pid = pid;
    hi.Type = 0x7FFu;
    DWORD written = 0;
    if(!WriteFile(h, &hi, sizeof(hi), &written, nullptr))
    {
        printf("[!] HidePid WriteFile failed Win32=%lu written=%lu\n", GetLastError(), written);
        CloseHandle(h);
        return 3;
    }
    printf("[+] HidePid written=%lu Type=0x%08X\n", written, hi.Type);

    hi.Command = UnhidePid;
    if(!WriteFile(h, &hi, sizeof(hi), &written, nullptr))
    {
        printf("[!] UnhidePid WriteFile failed Win32=%lu\n", GetLastError());
        CloseHandle(h);
        return 4;
    }
    printf("[+] UnhidePid written=%lu\n", written);

    CloseHandle(h);
    printf("[+] smoke OK\n");
    return 0;
}
