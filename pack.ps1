param(
    [string]$Version = "1.0.0"
)

Write-Host "📦 Packing xNVSE plugin for Mod Manager..." -ForegroundColor Cyan

# Build (Release Win32)
& .\quick-build.ps1
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$dll = Join-Path $PSScriptRoot "Release/MemoryPoolNVSE_RPmalloc.dll"
if (!(Test-Path $dll)) { throw "Missing DLL: $dll" }

# Staging structure: Data/NVSE/Plugins
$staging = Join-Path $PSScriptRoot "package_tmp"
Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue | Out-Null
$pluginsDir = Join-Path $staging "Data/NVSE/Plugins"
New-Item -ItemType Directory -Force -Path $pluginsDir | Out-Null

# Copy files
Copy-Item $dll (Join-Path $pluginsDir "MemoryPoolNVSE_RPmalloc.dll") -Force
# Optional: include default INI if present
$ini = Join-Path $PSScriptRoot "Data/NVSE/Plugins/RPNVSEOverdrive.ini"
if (Test-Path $ini) {
  Copy-Item $ini (Join-Path $pluginsDir "RPNVSEOverdrive.ini") -Force
}

# Output archive
$dist = Join-Path $PSScriptRoot "dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$zip = Join-Path $dist ("MemoryPoolNVSE_Overdrive_xNVSE_v{0}.zip" -f $Version)
Remove-Item $zip -Force -ErrorAction SilentlyContinue | Out-Null

Compress-Archive -Path (Join-Path $staging "Data") -DestinationPath $zip -CompressionLevel Optimal

$size = (Get-Item $zip).Length
Write-Host "✅ Created archive:" -ForegroundColor Green
Write-Host "   $zip" -ForegroundColor Gray
Write-Host ("   Size: {0:N0} bytes" -f $size) -ForegroundColor Gray
