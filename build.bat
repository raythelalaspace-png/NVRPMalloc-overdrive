@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
cd /d "C:\Users\NRO\source\repos\RPNVSE Overdrive"
msbuild MemoryPoolNVSE_RPmalloc.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /m
endlocal