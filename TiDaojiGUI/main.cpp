#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <utility>
#include "resource.h"
#include "..\TiDaoji\TiDaoji.h"

static HINSTANCE hInst;
static char iniPath[MAX_PATH];

static std::pair<int, HIDE_TYPE> gOptions[] =
{
    { IDC_CHK_PROCESSDEBUGFLAGS, HideProcessDebugFlags },
    { IDC_CHK_PROCESSDEBUGPORT, HideProcessDebugPort },
    { IDC_CHK_PROCESSDEBUGOBJECTHANDLE, HideProcessDebugObjectHandle },
    { IDC_CHK_DEBUGOBJECT, HideDebugObject },
    { IDC_CHK_SYSTEMDEBUGGERINFORMATION, HideSystemDebuggerInformation },
    { IDC_CHK_NTCLOSE, HideNtClose },
    { IDC_CHK_THREADHIDEFROMDEBUGGER, HideThreadHideFromDebugger },
    { IDC_CHK_NTGETCONTEXTTHREAD, HideNtGetContextThread },
    { IDC_CHK_NTSETCONTEXTTHREAD, HideNtSetContextThread },
    { IDC_CHK_NTSYSTEMDEBUGCONTROL, HideNtSystemDebugControl },
    { IDC_CHK_NTSYSTEMVMINFORMATION, HideNtSystemVMInformation },
};

static ULONG GetTypeDword(HWND hwndDlg)
{
    ULONG Option = 0;
    for(const auto& option : gOptions)
    {
        if(IsDlgButtonChecked(hwndDlg, option.first))
            Option |= (ULONG)option.second;
    }
    return Option;
}

static void ApplyTypeDword(HWND hwndDlg, ULONG type)
{
    for(const auto& option : gOptions)
    {
        CheckDlgButton(hwndDlg, option.first,
                       (type & (ULONG)option.second) ? BST_CHECKED : BST_UNCHECKED);
    }
}

static void SaveUiState(HWND hwndDlg)
{
    char driverName[256] = "";
    GetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_DRIVER), driverName, sizeof(driverName));
    WritePrivateProfileStringA("TiDaoji", "DriverName", driverName, iniPath);

    char typeBuf[32];
    sprintf_s(typeBuf, "0x%08X", GetTypeDword(hwndDlg));
    WritePrivateProfileStringA("TiDaoji", "Type", typeBuf, iniPath);

    char pidBuf[32];
    GetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_PID), pidBuf, sizeof(pidBuf));
    WritePrivateProfileStringA("TiDaoji", "LastPid", pidBuf, iniPath);
}

static void ShowWin32Error(HWND hwndDlg, const char* title)
{
    const DWORD err = GetLastError();
    char* sys = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, (LPSTR)&sys, 0, nullptr);
    char buf[512];
    if(sys)
        sprintf_s(buf, "%s\nWin32=%lu\n%s", title, err, sys);
    else
        sprintf_s(buf, "%s\nWin32=%lu", title, err);
    if(sys)
        LocalFree(sys);
    MessageBoxA(hwndDlg, buf, "TiDaojiGUI", MB_ICONERROR);
}

static void TiDaojiCall(HWND hwndDlg, HIDE_COMMAND Command)
{
    SaveUiState(hwndDlg);

    char driverName[256] = "\\\\.\\";
    GetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_DRIVER), driverName + 4, sizeof(driverName) - 4);
    if(driverName[4] == '\0')
    {
        MessageBoxA(hwndDlg, "Driver name empty (default TiDaoji)", "TiDaojiGUI", MB_ICONERROR);
        return;
    }

    HANDLE hDevice = CreateFileA(driverName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(hDevice == INVALID_HANDLE_VALUE)
    {
        ShowWin32Error(hwndDlg,
                       "Could not open device.\nIs TiDaoji.sys started?\nSee docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md");
        return;
    }

    HIDE_INFO HideInfo;
    HideInfo.Command = Command;
    HideInfo.Pid = GetDlgItemInt(hwndDlg, IDC_EDT_PID, 0, FALSE);
    HideInfo.Type = GetTypeDword(hwndDlg);
    DWORD written = 0;
    if(WriteFile(hDevice, &HideInfo, sizeof(HIDE_INFO), &written, 0))
    {
        char ok[160];
        const char* cmd =
            Command == HidePid ? "HidePid" :
            Command == UnhidePid ? "UnhidePid" : "UnhideAll";
        sprintf_s(ok, "%s OK\nPID=%lu Type=0x%08X\nDevice %s",
                  cmd, HideInfo.Pid, HideInfo.Type, driverName);
        MessageBoxA(hwndDlg, ok, "TiDaojiGUI", MB_ICONINFORMATION);
    }
    else
        ShowWin32Error(hwndDlg, "WriteFile failed");
    CloseHandle(hDevice);
}

static BOOL CALLBACK DlgMain(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch(uMsg)
    {
    case WM_INITDIALOG:
    {
        // Defaults then overlay ini
        SetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_DRIVER), "TiDaoji");
        ApplyTypeDword(hwndDlg, 0x7FFu);

        char driverName[256] = "";
        GetPrivateProfileStringA("TiDaoji", "DriverName", "TiDaoji", driverName, sizeof(driverName), iniPath);
        SetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_DRIVER), driverName);

        char typeStr[32] = "";
        GetPrivateProfileStringA("TiDaoji", "Type", "", typeStr, sizeof(typeStr), iniPath);
        if(typeStr[0] != '\0')
        {
            ULONG type = 0;
            if(sscanf_s(typeStr, "%i", (int*)&type) == 1 || sscanf_s(typeStr, "0x%x", &type) == 1)
                ApplyTypeDword(hwndDlg, type);
        }

        char pidStr[32] = "";
        GetPrivateProfileStringA("TiDaoji", "LastPid", "", pidStr, sizeof(pidStr), iniPath);
        if(pidStr[0] != '\0')
            SetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_PID), pidStr);

        SetWindowTextA(hwndDlg, "TiDaojiGUI (PR4) — NOT PG-safe; no dual-IH with CR");
    }
    return TRUE;

    case WM_CLOSE:
        SaveUiState(hwndDlg);
        EndDialog(hwndDlg, 0);
        return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wParam))
        {
        case IDC_BTN_HIDE:
            TiDaojiCall(hwndDlg, HidePid);
            return TRUE;
        case IDC_BTN_UNHIDE:
            TiDaojiCall(hwndDlg, UnhidePid);
            return TRUE;
        case IDC_BTN_UNHIDEALL:
            TiDaojiCall(hwndDlg, UnhideAll);
            return TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);
    hInst = hInstance;
    InitCommonControls();
    GetModuleFileNameA(hInstance, iniPath, sizeof(iniPath));
    auto ext = strrchr(iniPath, '.');
    if(ext != nullptr)
        *ext = '\0';
    strncat_s(iniPath, ".ini", _TRUNCATE);
    return (int)DialogBox(hInst, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)DlgMain);
}
