cd ..\bin
editbin /nologo /subsystem:console openttd.exe
cscript /nologo ai\regression\run.vbs
set RESULT=%ERRORLEVEL%
editbin /nologo /subsystem:windows openttd.exe
exit %RESULT%
