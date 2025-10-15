# Quick Development Build for MemoryPoolNVSE Overdrive
Write-Host "🔨 Quick Build..." -ForegroundColor Yellow

$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

if (Test-Path $msbuild) {
    & $msbuild "MemoryPoolNVSE_RPmalloc.vcxproj" /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
    
    if ($LASTEXITCODE -eq 0) {
        $dll = Get-Item "Release\MemoryPoolNVSE_RPmalloc.dll"
        Write-Host "✅ Built: $($dll.Length) bytes" -ForegroundColor Green
    } else {
        Write-Host "❌ Build failed!" -ForegroundColor Red
    }
} else {
    Write-Host "❌ MSBuild not found!" -ForegroundColor Red
}