@echo off
rem Build the third-party libraries of openMSX 21.0 with MSVC (x64, Release)
rem Usage: build3rd.cmd [openMSX source folder], default D:\openMSX_build\openMSX-21.0
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set BAUM=%~1
if "%BAUM%"=="" set BAUM=D:\openMSX_build\openMSX-21.0
cd /d "%BAUM%"
msbuild /m /nologo /v:m /p:Configuration=Release /p:Platform=x64 build\3rdparty\3rdparty.sln
echo BUILD END error code %ERRORLEVEL%
