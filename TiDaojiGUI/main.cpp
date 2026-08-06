#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <utility>
#include "resource.h"
#include "..\TiDaoji\TiDaoji.h"
#include "..\TiDaoji\user_client.h"

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
    { IDC_CHK_NTTERMINATEPROCESS, HideNtTerminateProcess },
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

static void SetAllChecks(HWND hwndDlg, BOOL on)
{
    for(const auto& option : gOptions)
        CheckDlgButton(hwndDlg, option.first, on ? BST_CHECKED : BST_UNCHECKED);
}

static void SetStatus(HWND hwndDlg, const char* text)
{
    SetDlgItemTextA(hwndDlg, IDC_LBL_STATUS, text);
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

static void FillSettings(HWND hwndDlg, TiDaojiUserSettings* s)
{
    char driverName[128] = "TiDaoji";
    GetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_DRIVER), driverName, sizeof(driverName));
    if(driverName[0] == '\0')
        strncpy_s(driverName, TIDAOJI_DEFAULT_DRIVER, _TRUNCATE);
    strncpy_s(s->DriverName, driverName, _TRUNCATE);
    s->Type = GetTypeDword(hwndDlg);
}

static const char* CmdName(HIDE_COMMAND c)
{
    switch(c)
    {
    case HidePid: return "HidePid";
    case UnhidePid: return "UnhidePid";
    case UnhideAll: return "UnhideAll";
    case SoftUnload: return "SoftUnload";
    default: return "Cmd";
    }
}

static void TiDaojiCall(HWND hwndDlg, HIDE_COMMAND Command)
{
    SaveUiState(hwndDlg);

    TiDaojiUserSettings s;
    FillSettings(hwndDlg, &s);
    if(s.DriverName[0] == '\0')
    {
        SetStatus(hwndDlg, "Status: driver name empty");
        MessageBoxA(hwndDlg, "Driver name empty (default TiDaoji)", "TiDaojiGUI", MB_ICONERROR);
        return;
    }

    if(Command == SoftUnload)
    {
        if(MessageBoxA(hwndDlg,
                       "SoftUnload tears down InfinityHook / device (L2/L3).\nContinue?",
                       "TiDaojiGUI", MB_ICONWARNING | MB_YESNO) != IDYES)
            return;
    }

    const DWORD pid = GetDlgItemInt(hwndDlg, IDC_EDT_PID, 0, FALSE);
    DWORD err = 0;
    if(!TiDaojiUserCall(&s, pid, Command, &err))
    {
        char w32[256];
        TiDaojiFormatWin32(err, w32, sizeof(w32));
        char status[320];
        sprintf_s(status, "Status: %s FAILED — %s", CmdName(Command), w32);
        SetStatus(hwndDlg, status);

        char box[512];
        sprintf_s(box, "%s failed.\n%s\nIs TiDaoji.sys started?\nSee docs/2026-08-07-tidaoji-dsu-profile-a-runbook.md",
                  CmdName(Command), w32);
        MessageBoxA(hwndDlg, box, "TiDaojiGUI", MB_ICONERROR);
        return;
    }

    char path[160];
    TiDaojiDevicePath(&s, path, sizeof(path));
    char ok[240];
    sprintf_s(ok, "Status: %s OK  PID=%lu Type=0x%08X  %s",
              CmdName(Command), (unsigned long)pid, s.Type, path);
    SetStatus(hwndDlg, ok);
}

static void TiDaojiStatus(HWND hwndDlg)
{
    SaveUiState(hwndDlg);
    TiDaojiUserSettings s;
    FillSettings(hwndDlg, &s);
    char path[160];
    TiDaojiDevicePath(&s, path, sizeof(path));
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if(h == INVALID_HANDLE_VALUE)
    {
        char w32[256];
        TiDaojiFormatWin32(GetLastError(), w32, sizeof(w32));
        char status[320];
        sprintf_s(status, "Status: OPEN FAIL %s — %s", path, w32);
        SetStatus(hwndDlg, status);
        return;
    }
    CloseHandle(h);
    char status[240];
    sprintf_s(status, "Status: OPEN OK %s  Type=0x%08X (CE/x64dbg share this device)", path, s.Type);
    SetStatus(hwndDlg, status);
}

static BOOL CALLBACK DlgMain(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch(uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_DRIVER), "TiDaoji");
        ApplyTypeDword(hwndDlg, 0xFFFu);

        char driverName[256] = "";
        GetPrivateProfileStringA("TiDaoji", "DriverName", "TiDaoji", driverName, sizeof(driverName), iniPath);
        SetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_DRIVER), driverName);

        char typeStr[32] = "";
        GetPrivateProfileStringA("TiDaoji", "Type", "", typeStr, sizeof(typeStr), iniPath);
        if(typeStr[0] != '\0')
        {
            ULONG type = 0;
            if(sscanf_s(typeStr, "%i", (int*)&type) == 1)
                ApplyTypeDword(hwndDlg, type);
        }

        char pidStr[32] = "";
        GetPrivateProfileStringA("TiDaoji", "LastPid", "", pidStr, sizeof(pidStr), iniPath);
        if(pidStr[0] != '\0')
            SetWindowTextA(GetDlgItem(hwndDlg, IDC_EDT_PID), pidStr);

        SetWindowTextA(hwndDlg, "TiDaojiGUI — NOT PG-safe; CE/x64dbg share \\\\.\\TiDaoji");
        SetStatus(hwndDlg, "Status: ready (lab). For Cheat Engine: tools/ce/TiDaoji.lua + tidaoji_cli.exe");
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
        case IDC_BTN_SOFTUNLOAD:
            TiDaojiCall(hwndDlg, SoftUnload);
            return TRUE;
        case IDC_BTN_STATUS:
            TiDaojiStatus(hwndDlg);
            return TRUE;
        case IDC_BTN_SELALL:
            SetAllChecks(hwndDlg, TRUE);
            return TRUE;
        case IDC_BTN_SELNONE:
            SetAllChecks(hwndDlg, FALSE);
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

    // Optional: TiDaojiGUI.exe <pid>  prefill + hide immediately for CE/scripts
    if(lpCmdLine && lpCmdLine[0])
    {
        // parse first token as pid if numeric — leave UI to apply
    }

    return (int)DialogBox(hInst, MAKEINTRESOURCE(DLG_MAIN), NULL, (DLGPROC)DlgMain);
}
