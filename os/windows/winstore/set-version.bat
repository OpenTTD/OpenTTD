@echo off
if [%1]==[] goto err

powershell -File "%~dp0set-version.ps1" %1 > "%temp%\ottd-set-version.bat"
if not errorlevel 0 goto err
call "%temp%\ottd-set-version.bat"
del /q "%temp%\ottd-set-version.bat"

@rem Version number will now be in %OTTD_VERSION%

goto :eof

:err
echo Please supply the path of openttd.exe as the argument to this batch file.
