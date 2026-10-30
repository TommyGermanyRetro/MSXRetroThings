@echo off
rem *************************************************************************************************
rem * build.cmd - build slottest.exe with MSVC (Visual Studio 2022, x64)                            *
rem *************************************************************************************************
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
cl /nologo /O2 /EHsc /W4 slottest.cpp /link
