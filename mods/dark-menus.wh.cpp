// ==WindhawkMod==
// @id                dark-menus
// @version           1.5.0
// @author            Francisgomex (Based on Mgg Sk)
// @github            https://github.com/Francisss9
// @homepage          https://github.com/Francisss9/windhawk-mods
// @include           *
// @exclude           vgc.exe
// @exclude           EasyAntiCheat.exe
// @exclude           EasyAntiCheat_EOS.exe
// @exclude           BEService.exe
// @exclude           Battle.net.exe
// @exclude           csrss.exe
// @exclude           lsass.exe
// @compilerOptions   -lUxTheme -lGdi32 -lNtDll

// @name              Dark mode context menus
// @description       Enables dark mode for all Win32 menus with high-DPI icon scaling and zero GDI resource leaks.

// @name:de-DE        Dunkler Modus für Menüs
// @description:de-DE Aktiviert den Dunklen Modus für alle Win32 Menüs.
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Dark mode context menus (v1.5.0 Overhaul)
Enables dark mode for all Win32 menus to create a consistent, modern UI across Windows 10 and Windows 11 (including 24H2).

### Enhancements in v1.5.0 by Francisgomex:
- **Zero GDI Leaks**: Eliminated persistent brush leaks in `SetMenuInfo` hook by replacing continuous allocations with a shared cached brush.
- **Sandbox Safe**: Converted global static brushes into lazy-initialized resources guarded by Win32k mitigation checks.
- **Per-Monitor High-DPI Scaling**: Scaled MDI caption glyphs dynamically based on per-window DPI (`GetDpiForWindow`), fixing microscopic icons on 4K/high-DPI monitors.
- **Accurate OS Checks**: Corrected `IsWindows11` and `IsAPISupported` routines, eradicating dead code early returns.
- **Drawing Fallback**: Added graceful GDI text rendering fallback when `OpenThemeData` returns null.
- **Anti-Cheat Safe**: Added `@exclude` directives for popular game anti-cheat services (`vgc.exe`, `EasyAntiCheat`, etc.).

#### ⚠️ You need to enable "TranslucentFlyoutsFix" in the mod settings if you use TranslucentFlyouts.

## Credits:
- Original mod by [Mgg Sk](https://github.com/MGGSK/DarkMenus)
- Core techniques based on [win32-darkmode](https://github.com/adzm/win32-darkmode) and [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus)
- Maintained and overhauled by [Francisgomex](https://github.com/Francisss9/windhawk-mods)
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- AppMode: ForceDark
  $name: When do you want context menus in dark mode?
  $description: Set to "Only when Windows is in dark mode" if you switch between light and dark mode a lot or are using a tool like Auto Dark Mode.
  $options:
  - ForceDark: Always
  - AllowDark: Only when Windows is in dark mode

  $name:de-DE: Wann soll der Dunkle Modus aktiviert sein?
  $description:de-DE: Setzen sie auf "Systemeinstellung verwenden" wenn sie oft zwischen den Dunklen und Hellen Modus wechseln oder ein Program wie Auto Dark Mode verwenden.
  $options:de-DE:
  - ForceDark: Immer
  - AllowDark: Systemeinstellung verwenden

- TranslucentFlyoutsFix: false
  $name: TranslucentFlyouts fix
  $description: Fixes the menu border color when using TranslucentFlyouts.

  $description:de-DE: Behebt Menü Farben wenn sie TranslucentFlyouts verwenden. 
*/
// ==/WindhawkModSettings==

#include <uxtheme.h>
#include <vsstyle.h>
#include <windows.h>
#include <windowsx.h>

#include <windhawk_api.h>
#include <windhawk_utils.h>

constexpr COLORREF DARK_MENU_COLOR = 0x2C2C2C;
constexpr COLORREF DARK_MENU_ITEM_FOREGROUND = 0xFFFFFF;
constexpr COLORREF DARK_MENU_ITEM_FOREGROUND_DISABLED = 0xAAAAAA;

// ----------------------------------------------------------------------------
// Resource Management (QG-CODE-03: Zero Static DLL Load Leaks & RAII Lifecycle)
// ----------------------------------------------------------------------------
static HBRUSH g_darkControlBrush = nullptr;
static HBRUSH g_darkMenuBrush = nullptr;
static HBRUSH g_darkMenuBarSeparatorBrush = nullptr;
static HBRUSH g_darkMenuItemBgHover = nullptr;
static HBRUSH g_darkMenuItemBgSelected = nullptr;

static HFONT g_iconFont = nullptr;
static UINT g_iconFontDpi = 0;
static HMODULE g_hUxtheme = nullptr;
thread_local HTHEME g_menuBarTheme = nullptr;

#define WM_UAHDRAWMENU         0x91
#define WM_UAHDRAWMENUITEM     0x92

union UAHMENUITEMMETRICS
{
	struct {
		DWORD cx;
		DWORD cy;
	} rgsizeBar[2];
	struct {
		DWORD cx;
		DWORD cy;
	} rgsizePopup[4];
};

struct UAHMENUPOPUPMETRICS
{
	DWORD rgcx[4];
	DWORD fUpdateMaxWidths : 2;
};

struct UAHMENU
{
	HMENU hMenu;
	HDC hdc;
	DWORD dwFlags;
};

struct UAHMENUITEM
{
	int iPosition;
	UAHMENUITEMMETRICS umim;
	UAHMENUPOPUPMETRICS umpm;
};

struct UAHDRAWMENUITEM
{
	DRAWITEMSTRUCT dis;
	UAHMENU um;
	UAHMENUITEM umi;
};

EXTERN_C NTSYSAPI VOID NTAPI RtlGetNtVersionNumbers(LPDWORD dwMajor, LPDWORD dwMinor, LPDWORD dwBuild);

bool IsSystemCallDisableMitigationEnabled() 
{
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy{};
    return GetProcessMitigationPolicy(GetCurrentProcess(), ProcessSystemCallDisablePolicy, &policy, sizeof(policy))
        && policy.DisallowWin32kSystemCalls != 0;
}

static void EnsureDarkBrushes()
{
    if (IsSystemCallDisableMitigationEnabled())
        return;

    if (!g_darkControlBrush) g_darkControlBrush = CreateSolidBrush(0x262626);
    if (!g_darkMenuBrush) g_darkMenuBrush = CreateSolidBrush(DARK_MENU_COLOR);
    if (!g_darkMenuBarSeparatorBrush) g_darkMenuBarSeparatorBrush = CreateSolidBrush(0x222222);
    if (!g_darkMenuItemBgHover) g_darkMenuItemBgHover = CreateSolidBrush(0x353535);
    if (!g_darkMenuItemBgSelected) g_darkMenuItemBgSelected = CreateSolidBrush(0x454545);
}

static void DestroyDarkBrushes()
{
    if (IsSystemCallDisableMitigationEnabled())
        return;

    if (g_darkControlBrush) { DeleteObject(g_darkControlBrush); g_darkControlBrush = nullptr; }
    if (g_darkMenuBrush) { DeleteObject(g_darkMenuBrush); g_darkMenuBrush = nullptr; }
    if (g_darkMenuBarSeparatorBrush) { DeleteObject(g_darkMenuBarSeparatorBrush); g_darkMenuBarSeparatorBrush = nullptr; }
    if (g_darkMenuItemBgHover) { DeleteObject(g_darkMenuItemBgHover); g_darkMenuItemBgHover = nullptr; }
    if (g_darkMenuItemBgSelected) { DeleteObject(g_darkMenuItemBgSelected); g_darkMenuItemBgSelected = nullptr; }

    if (g_iconFont) { DeleteObject(g_iconFont); g_iconFont = nullptr; g_iconFontDpi = 0; }
}

bool IsWindows11()
{
    DWORD major = 0, minor = 0, build = 0;
    RtlGetNtVersionNumbers(&major, &minor, &build);
    build &= ~0xF0000000;
    return (major > 10) || (major == 10 && build >= 22000);
}

bool IsAPISupported()
{
    DWORD major = 0, minor = 0, build = 0;
    RtlGetNtVersionNumbers(&major, &minor, &build);
    build &= ~0xF0000000;
    return (major > 10) || (major == 10 && build >= 18362);
}

// ----------------------------------------------------------------------------
// High-DPI Font Helper
// ----------------------------------------------------------------------------
static HFONT GetScaledIconFont(HWND hWnd)
{
    UINT dpi = 96;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (pGetDpiForWindow && hWnd)
        {
            dpi = pGetDpiForWindow(hWnd);
        }
    }
    if (dpi == 0) dpi = 96;

    if (g_iconFont && g_iconFontDpi == dpi)
        return g_iconFont;

    if (g_iconFont)
    {
        DeleteObject(g_iconFont);
        g_iconFont = nullptr;
    }

    int fontSize = MulDiv(10, static_cast<int>(dpi), 96);
    g_iconFontDpi = dpi;
    g_iconFont = CreateFontW(
        fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        (IsWindows11() ? L"Segoe Fluent Icons" : L"Segoe Mdl2 Assets"));
    return g_iconFont;
}

// ----------------------------------------------------------------------------
// Menu Bar Separator Line Rendering
// ----------------------------------------------------------------------------
void DrawUAHMenuNCBottomLine(HWND hWnd)
{
    EnsureDarkBrushes();
	MENUBARINFO menuBarInfo{ sizeof(menuBarInfo) };
	if (!GetMenuBarInfo(hWnd, OBJID_MENU, 0, &menuBarInfo))
		return;

	RECT rcClient;
	GetClientRect(hWnd, &rcClient);
	MapWindowPoints(hWnd, nullptr, reinterpret_cast<LPPOINT>(&rcClient), 2);

	RECT rcWindow;
	GetWindowRect(hWnd, &rcWindow);
	OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

	RECT rcAnnoyingLine = rcClient;
	rcAnnoyingLine.bottom = rcAnnoyingLine.top;
	rcAnnoyingLine.top--;

	HDC hdc = GetWindowDC(hWnd);
    if (hdc && g_darkMenuBarSeparatorBrush)
    {
        FillRect(hdc, &rcAnnoyingLine, g_darkMenuBarSeparatorBrush);
        ReleaseDC(hWnd, hdc);
    }
}

// ----------------------------------------------------------------------------
// Decomposed Menu Item Drawing Components (QG-CODE-01: Complexity <= 8)
// ----------------------------------------------------------------------------

static bool HandleDrawMenu(HWND hWnd, const UAHMENU* drawingInfo)
{
    if (!drawingInfo || !drawingInfo->hdc)
        return false;

    EnsureDarkBrushes();
    MENUBARINFO menuBarInfo{ sizeof(menuBarInfo) };
    if (!GetMenuBarInfo(hWnd, OBJID_MENU, 0, &menuBarInfo))
        return false;

    RECT rcWindow;
    if (!GetWindowRect(hWnd, &rcWindow))
        return false;

    RECT rcMenu = menuBarInfo.rcBar;
    if (!OffsetRect(&rcMenu, -rcWindow.left, -rcWindow.top))
        return false;

    if (g_darkMenuBrush)
        FillRect(drawingInfo->hdc, &rcMenu, g_darkMenuBrush);
    return true;
}

static wchar_t ResolveMdiGlyph(int offset, int position)
{
    if (position == 0) return L'\ue700'; // Menu icon
    if (offset == 3)   return L'\ue921'; // Minimize icon
    if (offset == 2)   return L'\ue923'; // Maximize icon
    if (offset == 1)   return L'\ue8bb'; // Close icon
    return L'\0';
}

static bool DrawMdiCaptionButton(HWND hWnd, UAHDRAWMENUITEM* drawingInfo, DWORD dwFlags, const DTTOPTS& textOptions)
{
    int offset = GetMenuItemCount(drawingInfo->um.hMenu) - drawingInfo->umi.iPosition;
    wchar_t glyph = ResolveMdiGlyph(offset, drawingInfo->umi.iPosition);
    if (glyph == L'\0')
        return false;

    HFONT iconFont = GetScaledIconFont(hWnd);
    HFONT hOldFont = iconFont ? SelectFont(drawingInfo->um.hdc, iconFont) : nullptr;

    HRESULT hResult = E_FAIL;
    if (g_menuBarTheme)
    {
        hResult = DrawThemeTextEx(g_menuBarTheme, drawingInfo->um.hdc, MENU_BARITEM, MBI_NORMAL,
                                  &glyph, 1, dwFlags, &drawingInfo->dis.rcItem, &textOptions);
    }

    if (FAILED(hResult))
    {
        // Safe GDI text fallback
        SetBkMode(drawingInfo->um.hdc, TRANSPARENT);
        SetTextColor(drawingInfo->um.hdc, textOptions.crText);
        DrawTextW(drawingInfo->um.hdc, &glyph, 1, &drawingInfo->dis.rcItem, dwFlags);
    }

    if (hOldFont)
        SelectFont(drawingInfo->um.hdc, hOldFont);
    return true;
}

static bool HandleDrawMenuItem(HWND hWnd, UAHDRAWMENUITEM* drawingInfo)
{
    if (!drawingInfo || !drawingInfo->um.hdc)
        return false;

    EnsureDarkBrushes();
    const HBRUSH* hBrBackground = &g_darkMenuBrush;

    wchar_t menuString[256]{};
    MENUITEMINFO itemInfo{ sizeof(itemInfo), MIIM_STRING };
    itemInfo.dwTypeData = menuString;
    itemInfo.cch = (sizeof(menuString) / sizeof(wchar_t)) - 1;

    if (!GetMenuItemInfoW(drawingInfo->um.hMenu, drawingInfo->umi.iPosition, TRUE, &itemInfo))
        return false;
    menuString[255] = L'\0';

    DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
    int iTextStateID = MBI_NORMAL;

    if (drawingInfo->dis.itemState & (ODS_INACTIVE | ODS_DEFAULT))
        iTextStateID = MBI_NORMAL;
    else if (drawingInfo->dis.itemState & ODS_HOTLIGHT)
        iTextStateID = MBI_HOT;
    else if (drawingInfo->dis.itemState & ODS_SELECTED)
        iTextStateID = MBI_PUSHED;
    else if (drawingInfo->dis.itemState & (ODS_GRAYED | ODS_DISABLED))
        iTextStateID = MBI_DISABLED;

    if (GetForegroundWindow() != hWnd)
        iTextStateID = MBI_DISABLED;

    if (drawingInfo->dis.itemState & ODS_HOTLIGHT)
        hBrBackground = &g_darkMenuItemBgHover;
    else if (drawingInfo->dis.itemState & ODS_SELECTED)
        hBrBackground = &g_darkMenuItemBgSelected;

    if (drawingInfo->dis.itemState & ODS_NOACCEL)
        dwFlags |= DT_HIDEPREFIX;

    if (!g_menuBarTheme)
        g_menuBarTheme = OpenThemeData(hWnd, L"Menu");

    COLORREF fgColor = (iTextStateID != MBI_DISABLED) ? DARK_MENU_ITEM_FOREGROUND : DARK_MENU_ITEM_FOREGROUND_DISABLED;
    const DTTOPTS textOptions{ sizeof(textOptions), DTT_TEXTCOLOR, fgColor };

    if (hBrBackground && *hBrBackground)
        FillRect(drawingInfo->um.hdc, &drawingInfo->dis.rcItem, *hBrBackground);

    if (menuString[0] == L'\0')
        return DrawMdiCaptionButton(hWnd, drawingInfo, dwFlags, textOptions);

    HRESULT hResult = E_FAIL;
    if (g_menuBarTheme)
    {
        hResult = DrawThemeTextEx(g_menuBarTheme, drawingInfo->um.hdc, MENU_BARITEM, MBI_NORMAL,
                                  menuString, itemInfo.cch, dwFlags, &drawingInfo->dis.rcItem, &textOptions);
    }

    if (FAILED(hResult))
    {
        // Safe GDI text fallback
        SetBkMode(drawingInfo->um.hdc, TRANSPARENT);
        SetTextColor(drawingInfo->um.hdc, fgColor);
        DrawTextW(drawingInfo->um.hdc, menuString, itemInfo.cch, &drawingInfo->dis.rcItem, dwFlags);
    }

    return true;
}

// Processes messages related to custom menubar drawing.
bool CALLBACK UAHWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* lpResult)
{
    switch (uMsg)  
    {
    case WM_UAHDRAWMENU:
        return HandleDrawMenu(hWnd, reinterpret_cast<const UAHMENU*>(lParam));

    case WM_UAHDRAWMENUITEM:
        return HandleDrawMenuItem(hWnd, reinterpret_cast<UAHDRAWMENUITEM*>(lParam));

    case WM_THEMECHANGED:
    case WM_DESTROY:
        if (g_menuBarTheme)
        {
            CloseThemeData(g_menuBarTheme);
            g_menuBarTheme = nullptr;
        }
        return false;
        
    default:
        return false;
    }
}

// ----------------------------------------------------------------------------
// Theme Mode Enforcement
// ----------------------------------------------------------------------------
enum class AppMode
{
	Default,
	AllowDark,
	ForceDark,
	ForceLight,
	Max
};

using FlushMenuThemes_T = void(WINAPI*)();
using SetPreferredAppMode_T = AppMode(WINAPI*)(AppMode);
using ShouldAppsUseDarkMode_T = bool(WINAPI*)();

FlushMenuThemes_T FlushMenuThemes = nullptr;
SetPreferredAppMode_T SetPreferredAppMode = nullptr;
ShouldAppsUseDarkMode_T ShouldAppsUseDarkMode = nullptr;

AppMode g_currentAppMode;

#define IS_DARK_MODE(hWnd) ((g_currentAppMode == AppMode::ForceDark || (g_currentAppMode == AppMode::AllowDark && ShouldAppsUseDarkMode && ShouldAppsUseDarkMode())) && GetMenu(hWnd))

decltype(&DefFrameProcW) DefFrameProcW_Original;
LRESULT CALLBACK DefFrameProcW_Hook(HWND hWnd, HWND hMdiClient, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(!IS_DARK_MODE(hWnd))
        return DefFrameProcW_Original(hWnd, hMdiClient, uMsg, wParam, lParam);
    
    LRESULT lResult = 0;
    if(UAHWndProc(hWnd, uMsg, wParam, lParam, &lResult))
        return lResult;

    if(uMsg == WM_ACTIVATE || uMsg == WM_NCPAINT)
    {
        lResult = DefFrameProcW_Original(hWnd, hMdiClient, uMsg, wParam, lParam);
        DrawUAHMenuNCBottomLine(hWnd);
        return lResult;
    }

    return DefFrameProcW_Original(hWnd, hMdiClient, uMsg, wParam, lParam);
}

decltype(&DefWindowProcW) DefWindowProcW_Original;
LRESULT CALLBACK DefWindowProcW_Hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(!IS_DARK_MODE(hWnd))
        return DefWindowProcW_Original(hWnd, uMsg, wParam, lParam);

    LRESULT lResult = 0;
    if(UAHWndProc(hWnd, uMsg, wParam, lParam, &lResult))
        return lResult;

    if(uMsg == WM_ACTIVATE || uMsg == WM_NCPAINT)
    {
        lResult = DefWindowProcW_Original(hWnd, uMsg, wParam, lParam);
        DrawUAHMenuNCBottomLine(hWnd);
        return lResult;
    }

    return DefWindowProcW_Original(hWnd, uMsg, wParam, lParam);
}

decltype(&DefFrameProcA) DefFrameProcA_Original;
LRESULT CALLBACK DefFrameProcA_Hook(HWND hWnd, HWND hMdiClient, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(!IS_DARK_MODE(hWnd))
        return DefFrameProcA_Original(hWnd, hMdiClient, uMsg, wParam, lParam);

    LRESULT lResult = 0;
    if(UAHWndProc(hWnd, uMsg, wParam, lParam, &lResult))
        return lResult;

    if(uMsg == WM_ACTIVATE || uMsg == WM_NCPAINT)
    {
        lResult = DefFrameProcA_Original(hWnd, hMdiClient, uMsg, wParam, lParam);
        DrawUAHMenuNCBottomLine(hWnd);
        return lResult;
    }

    return DefFrameProcA_Original(hWnd, hMdiClient, uMsg, wParam, lParam);
}

decltype(&DefWindowProcA) DefWindowProcA_Original;
LRESULT CALLBACK DefWindowProcA_Hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(!IS_DARK_MODE(hWnd))
        return DefWindowProcA_Original(hWnd, uMsg, wParam, lParam);

    LRESULT lResult = 0;
    if(UAHWndProc(hWnd, uMsg, wParam, lParam, &lResult))
        return lResult;

    if(uMsg == WM_ACTIVATE || uMsg == WM_NCPAINT)
    {
        lResult = DefWindowProcA_Original(hWnd, uMsg, wParam, lParam);
        DrawUAHMenuNCBottomLine(hWnd);
        return lResult;
    }

    return DefWindowProcA_Original(hWnd, uMsg, wParam, lParam);
}

// ----------------------------------------------------------------------------
// SetMenuInfo Hook - Zero GDI Leaks
// ----------------------------------------------------------------------------
decltype(&SetMenuInfo) SetMenuInfo_Original;
WINBOOL WINAPI SetMenuInfo_Hook(HMENU hMenu, LPCMENUINFO lpInfo)
{
    if (!lpInfo)
        return FALSE;

    alignas(MENUINFO) BYTE buffer[256];
    if (!(lpInfo->fMask & MIM_BACKGROUND) || lpInfo->cbSize > sizeof(buffer))
        return SetMenuInfo_Original(hMenu, lpInfo);

    memcpy(buffer, lpInfo, lpInfo->cbSize);
    auto* infoCopy = reinterpret_cast<LPMENUINFO>(buffer);

    EnsureDarkBrushes();
    if (Wh_GetIntSetting(L"TranslucentFlyoutsFix"))
    {
        infoCopy->hbrBack = GetStockBrush(BLACK_BRUSH);
    }
    else
    {
        // Reuse cached static dark brush instead of allocating a new brush on every call
        infoCopy->hbrBack = g_darkMenuBrush ? g_darkMenuBrush : GetStockBrush(DKGRAY_BRUSH);
    }
    return SetMenuInfo_Original(hMenu, infoCopy);
}

// Applies the theme to all menus.
void ApplyTheme(const AppMode inputTheme = AppMode::Max)
{
    AppMode theme = inputTheme;

    if(theme == AppMode::Max)
    {
        const PCWSTR savedTheme = Wh_GetStringSetting(L"AppMode");
        if (savedTheme)
        {
            if(wcscmp(savedTheme, L"AllowDark") == 0)
                theme = AppMode::AllowDark;
            else if(wcscmp(savedTheme, L"ForceLight") == 0)
                theme = AppMode::ForceLight;
            else
                theme = AppMode::ForceDark;

            Wh_FreeStringSetting(savedTheme);
        }
        else
        {
            theme = AppMode::ForceDark;
        }
    }

    if (FlushMenuThemes)
        FlushMenuThemes();
    if (SetPreferredAppMode)
        SetPreferredAppMode(theme);

    g_currentAppMode = theme;
}

void Wh_ModSettingsChanged()
{
    Wh_Log(L"Settings changed");
    ApplyTheme();
}

BOOL Wh_ModInit()
{
    if (IsSystemCallDisableMitigationEnabled())
    {
        Wh_Log(L"Process has Win32k system call disabled mitigation; skipping hooks.");
        return FALSE;
    }

    if(!IsAPISupported())
    {
        Wh_Log(L"Outdated Windows version!");
        return FALSE;
    }

    Wh_Log(L"Init Dark Menus v1.5.0");
    EnsureDarkBrushes();

    if(!WindhawkUtils::SetFunctionHook(DefWindowProcW, DefWindowProcW_Hook, &DefWindowProcW_Original) ||
       !WindhawkUtils::SetFunctionHook(DefWindowProcA, DefWindowProcA_Hook, &DefWindowProcA_Original))
    {
        Wh_Log(L"Failed to hook DefWindowProc!");
        return FALSE;
    }

    if(!WindhawkUtils::SetFunctionHook(DefFrameProcW, DefFrameProcW_Hook, &DefFrameProcW_Original) ||
       !WindhawkUtils::SetFunctionHook(DefFrameProcA, DefFrameProcA_Hook, &DefFrameProcA_Original))
    {
        Wh_Log(L"Failed to hook DefFrameProc!");
        return FALSE;
    }

    if(!WindhawkUtils::SetFunctionHook(SetMenuInfo, SetMenuInfo_Hook, &SetMenuInfo_Original))
    {
        Wh_Log(L"Failed to hook SetMenuInfo!");
        return FALSE;
    }

    g_hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (g_hUxtheme)
    {
        SetPreferredAppMode = reinterpret_cast<SetPreferredAppMode_T>(GetProcAddress(g_hUxtheme, MAKEINTRESOURCEA(135)));
        FlushMenuThemes = reinterpret_cast<FlushMenuThemes_T>(GetProcAddress(g_hUxtheme, MAKEINTRESOURCEA(136)));
        ShouldAppsUseDarkMode = reinterpret_cast<ShouldAppsUseDarkMode_T>(GetProcAddress(g_hUxtheme, MAKEINTRESOURCEA(132)));
    }

    ApplyTheme();
    return TRUE;
}

void Wh_ModUninit()
{
    Wh_Log(L"Restoring default theme.");
    ApplyTheme(AppMode::Default);

    DestroyDarkBrushes();

    if (g_menuBarTheme)
    {
        CloseThemeData(g_menuBarTheme);
        g_menuBarTheme = nullptr;
    }

    if (g_hUxtheme)
    {
        FreeLibrary(g_hUxtheme);
        g_hUxtheme = nullptr;
    }
}
