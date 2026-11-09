# Windhawk Mods Suite by Francisgomex

A curated collection of custom, high-performance, hardened mods for the [Windhawk](https://windhawk.net/) customization engine on Windows 11 (24H2+).

- **Author**: Francisgomex
- **GitHub**: [@Francisss9](https://github.com/Francisss9)
- **Engine**: Windhawk 1.7.3+ (Fully verified on Windows 11 Build 26100/26300 24H2)
- **Toolchain**: Clang 20 MinGW (`-std=c++23`, `-O2`, `-s`)

---

## 📦 Mods Included

### 1. `windows-11-taskbar-styler` (v1.11.1 - Compact Liquid Glass Overhaul)
Hardened and space-optimized overhaul of the Windows 11 Taskbar Styler.
- **Space Recovery**: Shrinks taskbar height from bloated 80px to native 48px, restoring **32px of vertical screen real estate** across all applications.
- **Balanced Geometry**: Replaces bloated 55x70px button bounds with sleek 40x40px capsules, saving **270px of horizontal workspace** (comfortably fits 20+ pinned/running apps).
- **Responsive System Tray**: Completely removes brittle hardcoded `TranslateTransform X="-435"` and negative margins; tray is now dynamically anchored to the right in its own liquid glass capsule.
- **Dock Animation Plus Synergy**: Injects `ClipToBounds=False` and `Canvas.ZIndex=1` for seamless, unclipped macOS-style icon zoom animations.
- **Automatic Fallback**: Out-of-the-box fallback ensures the compact Liquid Glass theme applies even if Windhawk settings start uninitialized.

### 2. `overhauled-alt-tab` (v2.0.0)
A modern, fluid, robust replacement for Windows Alt+Tab built with Direct2D, per-window DWM previews, low-latency input, and strict memory safety.
- **Key Enhancements**:
  - Independent cards per `HWND` (fixes Firefox/Chrome multi-window bundling).
  - Synchronous `SC_CLOSE` / `WM_CLOSE` immediate execution.
  - Cancel selection via `Alt + Esc`.
  - Zero GDI/COM leaks with RAII scope guards (`GdiSelectGuard`, `GdiHandleGuard`, `ComGuard`).
  - Multi-monitor, DPI scaling (`WM_DPICHANGED`), and DWM composition restart recovery.
  - Decomposed Direct2D renderer architecture ($\le 6$ cyclomatic complexity per sub-renderer).

### 3. `dark-menus` (v1.5.0)
Enables dark mode for all Win32 context and system menus with zero GDI leaks and anti-cheat protection.
- **Key Enhancements**:
  - Full UxTheme integration for native dark Win32 popup menus.
  - Automatic exclusion of anti-cheat and protected processes (`vgc.exe`, `EasyAntiCheat`, `BEService`).
  - High-DPI icon scaling and deterministic brush destruction on unhook.

### 4. `invisible-borders` (v1.1.0)
Removes the 1px accent border around Windows 11 windows for a cleaner, modern borderless aesthetic.
- **Key Enhancements**:
  - Multi-monitor DPI sync: Preserves borderless state across `WM_DPICHANGED` and monitor jumps.
  - Zero hot-path logging: Completely removes IPC spam on `WM_ACTIVATE` and `WM_NCACTIVATE`.
  - Proactive sandbox mitigation check via `IsSystemCallDisableMitigationEnabled()`.

### 5. `tray-hover-expand` (v1.7.2)
Automatically opens the hidden system tray icons flyout on hover without requiring a click.
- **Key Enhancements**:
  - Native Portuguese language keywords ("ícones ocultos", "mostrar ícones ocultos").
  - Multi-monitor and DPI scaling boundary synchronization on Windows 11 24H2.
  - Fullscreen detection guard: Prevents the flyout from appearing over games and full-screen video.

---

## 🛠️ Build & Verification System

Use `build.ps1` to compile any mod using Clang MinGW (`-std=c++23`):

```powershell
# Build any mod (generates 64-bit and 32-bit DLLs in build/):
.\build.ps1 -ModName "overhauled-alt-tab"
.\build.ps1 -ModName "dark-menus"
.\build.ps1 -ModName "invisible-borders"
.\build.ps1 -ModName "tray-hover-expand"
.\build.ps1 -ModName "windows-11-taskbar-styler"

# Build and copy directly to Windows Clipboard (ready to paste into Windhawk UI):
.\build.ps1 -ModName "windows-11-taskbar-styler" -Clipboard
```

---

## 📜 Quality Gates & Standards

All mods in this repository follow strict architectural invariants:
- **`fg-harden-mvp`**: Top 10 customer impact resilience audit and concise changelogs.
- **`fg-code-quality-audit`**: Cyclomatic complexity $\le 10$, deterministic RAII resource cleanup, and modern C++23 idioms.
- **`fg-resilient-testing`**: Automated 64-bit and 32-bit compilation verification across all mods before commit.
