# Windhawk Mods Suite by Francisgomex

Collection of custom, high-performance, hardened mods for the [Windhawk](https://windhawk.net/) customization engine on Windows 11.

- **Author**: Francisgomex
- **GitHub**: [@Francisss9](https://github.com/Francisss9)
- **Engine**: Windhawk 1.7.3+ (Windows 11 24H2 compatible)

---

## 📦 Mods Included

### 1. `overhauled-alt-tab` (v2.0.0)
A modern, fluid, robust replacement for Windows Alt+Tab built with Direct2D, per-window DWM previews, low-latency input, and full Windows 11 24H2 compatibility.
- **Key Enhancements**:
  - Independent cards per `HWND` (fixes Firefox/Chrome multi-window bundling).
  - Synchronous `SC_CLOSE` / `WM_CLOSE` immediate execution.
  - Cancel selection via `Alt + Esc`.
  - Zero GDI/COM leaks with RAII scope guards.
  - Multi-monitor, DPI scaling, and DWM composition restart recovery.
  - Decomposed Direct2D renderer architecture.

### 2. `dark-menus` (Under Overhaul)
Enables dark mode for all Win32 context and system menus with seamless Windows 11 uxtheme integration.

---

## 🛠️ Build Script

Use `build.ps1` to compile any mod for both **x86_64** (64-bit) and **i686** (32-bit) using Clang MinGW:

```powershell
# Build overhauled-alt-tab
.\build.ps1 -ModName "overhauled-alt-tab"

# Build and copy directly to Windows Clipboard (ready to paste into Windhawk Editor):
.\build.ps1 -ModName "overhauled-alt-tab" -Clipboard

# Build dark-menus
.\build.ps1 -ModName "dark-menus"
```
