@echo off
rem Fremdbibliotheken von openMSX 21.0 mit MSVC (x64, Release) bauen
rem Aufruf: build3rd.cmd [openMSX-Quellordner], Vorgabe D:\openMSX_build\openMSX-21.0
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set BAUM=%~1
if "%BAUM%"=="" set BAUM=D:\openMSX_build\openMSX-21.0
cd /d "%BAUM%"
msbuild /m /nologo /v:m /p:Configuration=Release /p:Platform=x64 build\3rdparty\3rdparty.sln
echo BUILD-ENDE Fehlercode %ERRORLEVEL%
