// ==WindhawkMod==
// @id              invisible-borders
// @name            Invisible Window Borders
// @description     Makes window borders invisible while keeping rounded corners, with high-DPI scaling and zero-overhead hooks.
// @version         1.1.0
// @author          Francisgomex (Based on Bo0ii)
// @github          https://github.com/Francisss9
// @homepage        https://github.com/Francisss9/windhawk-mods
// @include         *
// @exclude         devenv.exe
// @exclude         vgc.exe
// @exclude         EasyAntiCheat.exe
// @exclude         EasyAntiCheat_EOS.exe
// @exclude         BEService.exe
// @exclude         Battle.net.exe
// @exclude         csrss.exe
// @exclude         lsass.exe
// @compilerOptions -ldwmapi -luser32 -lNtDll
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Invisible Window Borders (v1.1.0 Overhaul)

Makes window borders completely invisible in Windows 11 while preserving native rounded corners and DWM drop shadows.

### Enhancements & Hardening in v1.1.0 by Francisgomex:
- **Zero Overhead on Hot Paths**: Removed synchronous `Wh_Log` spam from window activation and message loops, avoiding IPC bottlenecks.
- **Enhanced Window Lifecycle**: Extended message filtering to `WM_CREATE`, `WM_SHOWWINDOW`, and `WM_DPICHANGED`, ensuring invisible borders upon window instantiation and cross-monitor DPI movements without flicker.
- **Win32k Mitigation Safe**: Integrated `ProcessSystemCallDisablePolicy` detection to prevent crashes in sandboxed processes (Chromium renderers, Edge sandboxes).
- **Anti-Cheat Safe**: Added comprehensive exclusions for game security services (`vgc.exe`, `EasyAntiCheat`, `BEService`).
- **Modern Hook Standard**: Replaced legacy untyped hooks with type-safe `WindhawkUtils::SetFunctionHook`.
- **Smart Settings Updates**: Diff-guarded `Wh_ModSettingsChanged` avoids unnecessary bulk desktop enumeration cycles.

## Compatibility:
- Windows 11 (all versions, including 24H2)
- Requires Desktop Window Manager (DWM) with `DWMWA_BORDER_COLOR` support (Build 22000+).

## Credits & Attribution:
- Original mod by **[Bo0ii](https://github.com/Bo0ii)** ([windhawk-mods repository](https://github.com/Bo0ii/windhawk-mods)).
- Overhaul, hardening, and maintenance by **[Francisgomex](https://github.com/Francisss9)** in [windhawk-mods](https://github.com/Francisss9/windhawk-mods).

## License:
MIT License - Feel free to modify and distribute.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- SpecialWindows: false
  $name: Apply to Special Windows
  $description: Also apply to special windows like dialogs and tool windows.
*/
// ==/WindhawkModSettings==

#include <dwmapi.h>
#include <windows.h>
#include <windhawk_api.h>
#include <windhawk_utils.h>

#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif

#ifndef DWMWA_COLOR_DEFAULT
#define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#endif

EXTERN_C NTSYSAPI VOID NTAPI RtlGetNtVersionNumbers(LPDWORD dwMajor, LPDWORD dwMinor, LPDWORD dwBuild);

static bool g_specialWindows = false;

// ----------------------------------------------------------------------------
// Environment & Safety Checks
// ----------------------------------------------------------------------------
static bool IsDwmBorderColorSupported()
{
    DWORD major = 0, minor = 0, build = 0;
    RtlGetNtVersionNumbers(&major, &minor, &build);
    build &= ~0xF0000000;
    return (major > 10) || (major == 10 && build >= 22000);
}

static bool IsSystemCallDisableMitigationEnabled()
{
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy{};
    return GetProcessMitigationPolicy(GetCurrentProcess(), ProcessSystemCallDisablePolicy, &policy, sizeof(policy))
        && policy.DisallowWin32kSystemCalls != 0;
}

static bool IsValidTargetWindow(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd))
        return false;

    LONG_PTR dwStyle = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if ((dwStyle & WS_THICKFRAME) == WS_THICKFRAME || (dwStyle & WS_CAPTION) == WS_CAPTION)
        return true;

    return g_specialWindows && ((dwStyle & WS_OVERLAPPED) == WS_OVERLAPPED) && ((dwStyle & WS_POPUP) != WS_POPUP);
}

// ----------------------------------------------------------------------------
// DWM Border Manipulation
// ----------------------------------------------------------------------------
decltype(&DwmSetWindowAttribute) DwmSetWindowAttribute_orig = nullptr;

static void SetWindowBorderInvisible(HWND hWnd)
{
    if (!IsValidTargetWindow(hWnd) || !DwmSetWindowAttribute_orig)
        return;

    constexpr COLORREF kBorderInvisible = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute_orig(hWnd, DWMWA_BORDER_COLOR, &kBorderInvisible, sizeof(kBorderInvisible));
}

static void RestoreWindowBorderDefault(HWND hWnd)
{
    if (!IsValidTargetWindow(hWnd) || !DwmSetWindowAttribute_orig)
        return;

    constexpr COLORREF kColorDefault = DWMWA_COLOR_DEFAULT;
    DwmSetWindowAttribute_orig(hWnd, DWMWA_BORDER_COLOR, &kColorDefault, sizeof(kColorDefault));
}

HRESULT WINAPI DwmSetWindowAttribute_hook(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute)
{
    if (dwAttribute == DWMWA_BORDER_COLOR && IsValidTargetWindow(hWnd))
    {
        constexpr COLORREF kBorderInvisible = DWMWA_COLOR_NONE;
        return DwmSetWindowAttribute_orig(hWnd, dwAttribute, &kBorderInvisible, sizeof(kBorderInvisible));
    }
    return DwmSetWindowAttribute_orig(hWnd, dwAttribute, pvAttribute, cbAttribute);
}

// ----------------------------------------------------------------------------
// Window Message Handlers (Zero-Lag Non-Client Invalidation)
// ----------------------------------------------------------------------------
static void HandleBorderRefreshMessage(HWND hWnd, UINT uMsg)
{
    switch (uMsg)
    {
    case WM_CREATE:
    case WM_SHOWWINDOW:
    case WM_ACTIVATE:
    case WM_NCACTIVATE:
    case WM_DPICHANGED:
    case WM_DWMCOLORIZATIONCOLORCHANGED:
        SetWindowBorderInvisible(hWnd);
        break;
    default:
        break;
    }
}

decltype(&DefWindowProcW) DefWindowProcW_orig = nullptr;
LRESULT WINAPI DefWindowProcW_hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefWindowProcW_orig(hWnd, uMsg, wParam, lParam);
    HandleBorderRefreshMessage(hWnd, uMsg);
    return result;
}

decltype(&DefWindowProcA) DefWindowProcA_orig = nullptr;
LRESULT WINAPI DefWindowProcA_hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefWindowProcA_orig(hWnd, uMsg, wParam, lParam);
    HandleBorderRefreshMessage(hWnd, uMsg);
    return result;
}

decltype(&DefDlgProcW) DefDlgProcW_orig = nullptr;
LRESULT WINAPI DefDlgProcW_hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefDlgProcW_orig(hWnd, uMsg, wParam, lParam);
    if (uMsg == WM_NCACTIVATE || uMsg == WM_SHOWWINDOW)
    {
        SetWindowBorderInvisible(hWnd);
    }
    return result;
}

decltype(&DefDlgProcA) DefDlgProcA_orig = nullptr;
LRESULT WINAPI DefDlgProcA_hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = DefDlgProcA_orig(hWnd, uMsg, wParam, lParam);
    if (uMsg == WM_NCACTIVATE || uMsg == WM_SHOWWINDOW)
    {
        SetWindowBorderInvisible(hWnd);
    }
    return result;
}

// ----------------------------------------------------------------------------
// EnumWindows Callbacks
// ----------------------------------------------------------------------------
static BOOL CALLBACK ApplyBordersEnumProc(HWND hWnd, LPARAM lParam)
{
    DWORD targetPid = static_cast<DWORD>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hWnd, &windowPid);
    if (windowPid == targetPid)
    {
        SetWindowBorderInvisible(hWnd);
    }
    return TRUE;
}

static BOOL CALLBACK RestoreBordersEnumProc(HWND hWnd, LPARAM lParam)
{
    DWORD targetPid = static_cast<DWORD>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hWnd, &windowPid);
    if (windowPid == targetPid)
    {
        RestoreWindowBorderDefault(hWnd);
    }
    return TRUE;
}

// ----------------------------------------------------------------------------
// Windhawk Mod Lifecycle
// ----------------------------------------------------------------------------
BOOL Wh_ModInit()
{
    if (IsSystemCallDisableMitigationEnabled())
    {
        Wh_Log(L"Win32k system call disabled mitigation active; skipping hooks.");
        return FALSE;
    }

    if (!IsDwmBorderColorSupported())
    {
        Wh_Log(L"DWMWA_BORDER_COLOR requires Windows 11 Build 22000+");
        return FALSE;
    }

    Wh_Log(L"Init Invisible Borders v1.1.0");
    g_specialWindows = Wh_GetIntSetting(L"SpecialWindows") != 0;

    if (!WindhawkUtils::SetFunctionHook(DwmSetWindowAttribute, DwmSetWindowAttribute_hook, &DwmSetWindowAttribute_orig))
    {
        Wh_Log(L"Failed to hook DwmSetWindowAttribute");
        return FALSE;
    }

    if (!WindhawkUtils::SetFunctionHook(DefWindowProcW, DefWindowProcW_hook, &DefWindowProcW_orig) ||
        !WindhawkUtils::SetFunctionHook(DefWindowProcA, DefWindowProcA_hook, &DefWindowProcA_orig))
    {
        Wh_Log(L"Failed to hook DefWindowProc");
        return FALSE;
    }

    if (!WindhawkUtils::SetFunctionHook(DefDlgProcW, DefDlgProcW_hook, &DefDlgProcW_orig) ||
        !WindhawkUtils::SetFunctionHook(DefDlgProcA, DefDlgProcA_hook, &DefDlgProcA_orig))
    {
        Wh_Log(L"Failed to hook DefDlgProc");
        return FALSE;
    }

    return TRUE;
}

void Wh_ModAfterInit()
{
    EnumWindows(ApplyBordersEnumProc, static_cast<LPARAM>(GetCurrentProcessId()));
}

void Wh_ModBeforeUninit()
{
    EnumWindows(RestoreBordersEnumProc, static_cast<LPARAM>(GetCurrentProcessId()));
}

void Wh_ModSettingsChanged()
{
    bool newSpecial = Wh_GetIntSetting(L"SpecialWindows") != 0;
    if (newSpecial != g_specialWindows)
    {
        EnumWindows(RestoreBordersEnumProc, static_cast<LPARAM>(GetCurrentProcessId()));
        g_specialWindows = newSpecial;
        EnumWindows(ApplyBordersEnumProc, static_cast<LPARAM>(GetCurrentProcessId()));
    }
}
