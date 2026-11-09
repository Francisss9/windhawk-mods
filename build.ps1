param(
    [string]$ModName = "overhauled-alt-tab",
    [switch]$Clipboard
)

$ErrorActionPreference = "Stop"

$Clang = "C:\Program Files\Windhawk\Compiler\bin\clang++.exe"
$EngineVer = "1.7.3"
$IncludeDir = "C:\Program Files\Windhawk\Compiler\include"
$LibDir64 = "C:\Program Files\Windhawk\Engine\$EngineVer\64"
$LibDir32 = "C:\Program Files\Windhawk\Engine\$EngineVer\32"

$ModFile = Join-Path $PSScriptRoot "mods\$ModName.wh.cpp"
if (-not (Test-Path $ModFile)) {
    $ModFile = Join-Path $PSScriptRoot "mods\$ModName"
    if (-not (Test-Path $ModFile)) {
        Write-Error "Mod file not found for '$ModName'. Available in mods/:"
        Get-ChildItem (Join-Path $PSScriptRoot "mods") -Filter "*.wh.cpp" | Select-Object -ExpandProperty Name
        exit 1
    }
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Building Windhawk Mod: $ModName" -ForegroundColor Cyan
Write-Host "Source: $ModFile" -ForegroundColor Gray
Write-Host "========================================" -ForegroundColor Cyan

# Read compiler options from mod header
$ModContent = Get-Content $ModFile -Raw
$CompilerFlags = @()
if ($ModContent -match '//\s*@compilerOptions\s+(.+)') {
    $extraFlags = $Matches[1].Trim() -split '\s+'
    $CompilerFlags += $extraFlags
    Write-Host "Extra Compiler Flags: $($extraFlags -join ' ')" -ForegroundColor DarkGray
}

$OutDir = Join-Path $PSScriptRoot "build"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$Out64 = Join-Path $OutDir "$ModName-x64.dll"
$Out32 = Join-Path $OutDir "$ModName-x86.dll"

$DefId = '-DWH_MOD_ID=L"' + $ModName + '"'
$DefVer = '-DWH_MOD_VERSION=L"2.0.0"'

Write-Host "[1/2] Compiling 64-bit (x86_64)..." -ForegroundColor Yellow
& $Clang `
    -std=c++23 -O2 -DUNICODE -D_UNICODE `
    -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 -DNTDDI_VERSION=0x0A000008 `
    -D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD `
    $DefId $DefVer `
    -x c++ $ModFile `
    -include (Join-Path $IncludeDir "windhawk_api.h") `
    -target x86_64-w64-mingw32 `
    -x none (Join-Path $LibDir64 "windhawk.lib") `
    -Wno-ignored-attributes -shared "-Wl,--kill-at" -s `
    $CompilerFlags `
    -o $Out64

if ($LASTEXITCODE -ne 0) {
    Write-Error "64-bit build failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}
Write-Host "  -> OK: $Out64" -ForegroundColor Green

if ($ModContent -match '//\s*@architecture\s+x86-64') {
    Write-Host "[2/2] Skipping 32-bit build (@architecture x86-64 specified)" -ForegroundColor DarkGray
} else {
    Write-Host "[2/2] Compiling 32-bit (i686)..." -ForegroundColor Yellow
    & $Clang `
        -std=c++23 -O2 -DUNICODE -D_UNICODE `
        -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 -DNTDDI_VERSION=0x0A000008 `
        -D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD `
        $DefId $DefVer `
        -x c++ $ModFile `
        -include (Join-Path $IncludeDir "windhawk_api.h") `
        -target i686-w64-mingw32 `
        -x none (Join-Path $LibDir32 "windhawk.lib") `
        -Wno-ignored-attributes -shared "-Wl,--kill-at" -s `
        $CompilerFlags `
        -o $Out32

    if ($LASTEXITCODE -ne 0) {
        Write-Error "32-bit build failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
    Write-Host "  -> OK: $Out32" -ForegroundColor Green
}

if ($Clipboard) {
    Get-Content $ModFile -Raw | Set-Clipboard
    Write-Host "`n[Clipboard] Mod source copied to Windows Clipboard!" -ForegroundColor Magenta
}

Write-Host "`nMod '$ModName' built successfully for both architectures!" -ForegroundColor Green
