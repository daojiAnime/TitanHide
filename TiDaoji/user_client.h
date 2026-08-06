#ifndef _TIDAOJI_USER_CLIENT_H
#define _TIDAOJI_USER_CLIENT_H

// Shared user-mode client for Olly / TitanEngine / tests (header-only).
// Keeps device path + HIDE_INFO write in one place. NOT for kernel code.

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "TiDaoji.h"

#ifndef TIDAOJI_DEFAULT_TYPE
// BIT(1)..BIT(12) inclusive (incl. NtTerminateProcess)
#define TIDAOJI_DEFAULT_TYPE 0xFFFu
#endif

#ifndef TIDAOJI_DEFAULT_DRIVER
#define TIDAOJI_DEFAULT_DRIVER "TiDaoji"
#endif

struct TiDaojiUserSettings
{
    char DriverName[128];
    ULONG Type;
};

inline void TiDaojiUserSettingsInit(TiDaojiUserSettings* s)
{
    memset(s, 0, sizeof(*s));
    strncpy_s(s->DriverName, TIDAOJI_DEFAULT_DRIVER, _TRUNCATE);
    s->Type = TIDAOJI_DEFAULT_TYPE;
}

// iniPath: full path to .ini (e.g. TiDaojiOlly.ini next to the DLL)
inline void TiDaojiUserSettingsLoad(TiDaojiUserSettings* s, const char* iniPath)
{
    TiDaojiUserSettingsInit(s);
    if(!iniPath || !iniPath[0])
        return;
    char name[128] = "";
    GetPrivateProfileStringA("TiDaoji", "DriverName", TIDAOJI_DEFAULT_DRIVER, name, sizeof(name), iniPath);
    if(name[0])
        strncpy_s(s->DriverName, name, _TRUNCATE);
    char typeStr[32] = "";
    GetPrivateProfileStringA("TiDaoji", "Type", "", typeStr, sizeof(typeStr), iniPath);
    if(typeStr[0])
    {
        unsigned int t = 0;
        if(sscanf_s(typeStr, "%i", (int*)&t) == 1)
            s->Type = (ULONG)t;
    }
}

inline void TiDaojiUserSettingsSave(const TiDaojiUserSettings* s, const char* iniPath)
{
    if(!iniPath || !s)
        return;
    WritePrivateProfileStringA("TiDaoji", "DriverName", s->DriverName, iniPath);
    char typeBuf[32];
    sprintf_s(typeBuf, "0x%08X", s->Type);
    WritePrivateProfileStringA("TiDaoji", "Type", typeBuf, iniPath);
}

// Module directory + baseName + ".ini" (baseName without extension preferred)
inline void TiDaojiIniPathFromModule(HINSTANCE hMod, const char* baseName, char* out, size_t outCch)
{
    char path[MAX_PATH] = "";
    GetModuleFileNameA(hMod, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if(slash)
        slash[1] = '\0';
    else
        path[0] = '\0';
    sprintf_s(out, outCch, "%s%s.ini", path, baseName);
}

inline void TiDaojiDevicePath(const TiDaojiUserSettings* s, char* out, size_t outCch)
{
    sprintf_s(out, outCch, "\\\\.\\%s", s->DriverName[0] ? s->DriverName : TIDAOJI_DEFAULT_DRIVER);
}

// Returns TRUE on WriteFile success. Optional errOut for GetLastError after failure.
inline BOOL TiDaojiUserCall(const TiDaojiUserSettings* s, DWORD pid, HIDE_COMMAND cmd, DWORD* errOut)
{
    if(errOut)
        *errOut = 0;
    char path[160];
    TiDaojiDevicePath(s, path, sizeof(path));
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(h == INVALID_HANDLE_VALUE)
    {
        if(errOut)
            *errOut = GetLastError();
        return FALSE;
    }
    HIDE_INFO info;
    info.Command = cmd;
    info.Pid = pid;
    info.Type = s ? s->Type : TIDAOJI_DEFAULT_TYPE;
    DWORD written = 0;
    BOOL ok = WriteFile(h, &info, sizeof(info), &written, 0);
    if(!ok && errOut)
        *errOut = GetLastError();
    CloseHandle(h);
    return ok;
}

inline void TiDaojiFormatWin32(DWORD err, char* out, size_t outCch)
{
    char* sys = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, (LPSTR)&sys, 0, nullptr);
    if(sys)
    {
        // trim
        size_t n = strlen(sys);
        while(n && (sys[n - 1] == '\r' || sys[n - 1] == '\n' || sys[n - 1] == ' '))
            sys[--n] = '\0';
        sprintf_s(out, outCch, "Win32=%lu (%s)", err, sys);
        LocalFree(sys);
    }
    else
        sprintf_s(out, outCch, "Win32=%lu", err);
}

#endif // _TIDAOJI_USER_CLIENT_H
