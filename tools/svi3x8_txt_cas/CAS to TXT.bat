@echo off
py "%~dp0svi_cas_to_txt.py" %*
if errorlevel 1 pause
