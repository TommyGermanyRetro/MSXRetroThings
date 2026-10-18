@echo off
py "%~dp0svi_txt_to_cas.py" %*
if errorlevel 1 pause
