@echo off
"c:\Program Files\NSIS\makensis.exe" /DVERSION_INCLUDE=version_win9x.txt install.nsi > win9x.log
"c:\Program Files\NSIS\makensis.exe" /DVERSION_INCLUDE=version_win32.txt install.nsi > win32.log
"c:\Program Files\NSIS\makensis.exe" /DVERSION_INCLUDE=version_win64.txt install.nsi > win64.log
