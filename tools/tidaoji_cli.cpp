// Minimal CLI for TiDaoji (CE autorun, scripts, automation).
// cl /O2 /EHsc /I.. /Fe:tidaoji_cli.exe tidaoji_cli.cpp
//
// usage:
//   tidaoji_cli hide <pid> [--type 0xFFF] [--driver TiDaoji]
//   tidaoji_cli unhide <pid> [--driver TiDaoji]
//   tidaoji_cli unhide-all [--driver TiDaoji]
//   tidaoji_cli soft-unload [--driver TiDaoji]
//   tidaoji_cli status [--driver TiDaoji]
//
// exit: 0 ok, 1 usage, 2 open fail, 3 write fail
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../TiDaoji/TiDaoji.h"
#include "../TiDaoji/user_client.h"

static void usage()
{
    printf("tidaoji_cli — TiDaoji user client (NOT PG-safe)\n");
    printf("  hide <pid> [--type 0xFFF] [--driver NAME]\n");
    printf("  unhide <pid> [--driver NAME]\n");
    printf("  unhide-all [--driver NAME]\n");
    printf("  soft-unload [--driver NAME]\n");
    printf("  status [--driver NAME]\n");
}

static bool parse_u32(const char* s, ULONG* out)
{
    if(!s || !out)
        return false;
    char* end = nullptr;
    unsigned long v = strtoul(s, &end, 0);
    if(end == s)
        return false;
    *out = (ULONG)v;
    return true;
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        usage();
        return 1;
    }

    TiDaojiUserSettings s;
    TiDaojiUserSettingsInit(&s);
    s.Type = 0xFFFu; // BIT1..BIT12

    const char* cmd = argv[1];
    ULONG pid = 0;
    int i = 2;

    if(_stricmp(cmd, "hide") == 0 || _stricmp(cmd, "unhide") == 0)
    {
        if(i >= argc || !parse_u32(argv[i], &pid))
        {
            usage();
            return 1;
        }
        i++;
    }

    for(; i < argc; i++)
    {
        if(_stricmp(argv[i], "--type") == 0 && i + 1 < argc)
        {
            ULONG t = 0;
            if(!parse_u32(argv[++i], &t))
            {
                fprintf(stderr, "[!] bad --type\n");
                return 1;
            }
            s.Type = t;
        }
        else if(_stricmp(argv[i], "--driver") == 0 && i + 1 < argc)
        {
            strncpy_s(s.DriverName, argv[++i], _TRUNCATE);
        }
        else if(_stricmp(argv[i], "-h") == 0 || _stricmp(argv[i], "--help") == 0)
        {
            usage();
            return 1;
        }
        else
        {
            fprintf(stderr, "[!] unknown arg: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    char path[160];
    TiDaojiDevicePath(&s, path, sizeof(path));

    if(_stricmp(cmd, "status") == 0)
    {
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if(h == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            char msg[256];
            TiDaojiFormatWin32(err, msg, sizeof(msg));
            printf("device=%s OPEN=fail %s\n", path, msg);
            return 2;
        }
        CloseHandle(h);
        printf("device=%s OPEN=ok type_default=0x%08X\n", path, s.Type);
        return 0;
    }

    HIDE_COMMAND hc;
    if(_stricmp(cmd, "hide") == 0)
        hc = HidePid;
    else if(_stricmp(cmd, "unhide") == 0)
        hc = UnhidePid;
    else if(_stricmp(cmd, "unhide-all") == 0)
        hc = UnhideAll;
    else if(_stricmp(cmd, "soft-unload") == 0)
        hc = SoftUnload;
    else
    {
        usage();
        return 1;
    }

    DWORD err = 0;
    if(!TiDaojiUserCall(&s, pid, hc, &err))
    {
        char msg[256];
        TiDaojiFormatWin32(err, msg, sizeof(msg));
        fprintf(stderr, "[!] %s failed pid=%lu type=0x%08X %s\n",
                cmd, (unsigned long)pid, s.Type, msg);
        return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ? 2 : 3;
    }

    printf("[+] %s ok pid=%lu type=0x%08X device=%s\n",
           cmd, (unsigned long)pid, s.Type, path);
    return 0;
}
