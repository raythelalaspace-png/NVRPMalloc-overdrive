# Deployment Script for MemoryPoolNVSE Overdrive
param(
    [string]$GamePath = "",
    [switch]$AutoDetect = $true
)

Write-Host "🚀 MemoryPoolNVSE Overdrive Deployment" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan

# Build first
Write-Host "Building plugin..." -ForegroundColor Yellow
& .\quick-build.ps1

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed, aborting deployment" -ForegroundColor Red
    exit 1
}

$dllSource = "Release\MemoryPoolNVSE_RPmalloc.dll"
if (!(Test-Path $dllSource)) {
    Write-Host "❌ DLL not found: $dllSource" -ForegroundColor Red
    exit 1
}

# Auto-detect game installation
$commonPaths = @(
    "C:\Steam\steamapps\common\Fallout New Vegas",
    "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas", 
    "C:\GOG Games\Fallout New Vegas",
    "C:\Program Files\Fallout New Vegas",
    "C:\Program Files (x86)\Fallout New Vegas"
)

$gamePath = $GamePath
if ($AutoDetect -and [string]::IsNullOrEmpty($gamePath)) {
    foreach ($path in $commonPaths) {
        if (Test-Path "$path\FalloutNV.exe") {
            $gamePath = $path
            Write-Host "✅ Auto-detected game at: $gamePath" -ForegroundColor Green
            break
        }
    }
}

if ([string]::IsNullOrEmpty($gamePath) -or !(Test-Path $gamePath)) {
    Write-Host "❌ Game not found. Please specify path:" -ForegroundColor Red
    Write-Host "   .\deploy.ps1 -GamePath 'C:\Your\Game\Path'" -ForegroundColor Yellow
    exit 1
}

# Check NVSE
$nvseLoader = "$gamePath\nvse_loader.exe"
if (!(Test-Path $nvseLoader)) {
    Write-Host "⚠️  NVSE not detected at: $nvseLoader" -ForegroundColor Yellow
    Write-Host "   Make sure NVSE is installed!" -ForegroundColor Yellow
}

# Create plugin directory
$pluginDir = "$gamePath\Data\NVSE\Plugins"
if (!(Test-Path $pluginDir)) {
    Write-Host "Creating plugin directory..." -ForegroundColor Yellow
    New-Item -Path $pluginDir -ItemType Directory -Force | Out-Null
}

# Deploy plugin
$dllTarget = "$pluginDir\MemoryPoolNVSE_RPmalloc.dll"
Write-Host "Deploying plugin..." -ForegroundColor Yellow
Copy-Item $dllSource $dllTarget -Force

$deployedDll = Get-Item $dllTarget
Write-Host "✅ Deployed successfully!" -ForegroundColor Green
Write-Host "   Location: $dllTarget" -ForegroundColor Gray
Write-Host "   Size: $($deployedDll.Length) bytes" -ForegroundColor Gray
Write-Host
Write-Host "🎮 Ready to launch!" -ForegroundColor Cyan
Write-Host "   Launch via: $nvseLoader" -ForegroundColor White
Write-Host
Write-Host "🔍 Verify with console commands:" -ForegroundColor Yellow
Write-Host "   IsMemoryPoolActive   (should return 1)" -ForegroundColor Gray
Write-Host "   IsMemoryPoolHooked   (should return 1)" -ForegroundColor Gray