// Minimal live smoke for TiDaoji.sys: open device, HidePid/UnhidePid self.
// Build (win-master): cl /O2 /EHsc /Fe:tidaoji_smoke.exe tidaoji_smoke.cpp
// Run elevated after sc start TiDaoji.
#include <windows.h>
#include <stdio.h>
#include "../TiDaoji/TiDaoji.h"

static int usage(const char* argv0)
{
    printf("usage: %s [driverName] [pid] [--soft-unload]\n", argv0);
    printf("  default driverName=TiDaoji  pid=current\n");
    printf("  --soft-unload  send SoftUnload (L2/L3 teardown)\n");
    return 1;
}

int main(int argc, char** argv)
{
    const char* name = "TiDaoji";
    DWORD pid = GetCurrentProcessId();
    bool softUnload = false;
    int argi = 1;
    if(argi < argc && argv[argi][0] != '-')
        name = argv[argi++];
    if(argi < argc && argv[argi][0] != '-')
        pid = (DWORD)strtoul(argv[argi++], nullptr, 0);
    for(; argi < argc; argi++)
    {
        if(strcmp(argv[argi], "--soft-unload") == 0)
            softUnload = true;
        else if(strcmp(argv[argi], "-h") == 0 || strcmp(argv[argi], "--help") == 0)
            return usage(argv[0]);
    }

    char path[160];
    sprintf_s(path, "\\\\.\\%s", name);
    printf("[*] open %s  pid=%lu (0x%lX)\n", path, (unsigned long)pid, (unsigned long)pid);

    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if(h == INVALID_HANDLE_VALUE)
    {
        printf("[!] CreateFile failed Win32=%lu\n", GetLastError());
        return 2;
    }
    printf("[+] device open OK\n");

    HIDE_INFO hi = {};
    DWORD written = 0;

    if(softUnload)
    {
        hi.Command = SoftUnload;
        hi.Pid = 0;
        hi.Type = 0;
        if(!WriteFile(h, &hi, sizeof(hi), &written, nullptr))
        {
            printf("[!] SoftUnload WriteFile failed Win32=%lu\n", GetLastError());
            CloseHandle(h);
            return 5;
        }
        printf("[+] SoftUnload written=%lu\n", written);
        CloseHandle(h);
        printf("[+] soft-unload OK (device should be gone)\n");
        return 0;
    }

    hi.Command = HidePid;
    hi.Pid = pid;
    hi.Type = 0xFFFu;
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
