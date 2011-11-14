@echo off
if "%1" == "" goto err

echo Removing NoSound...

del %1\baseset\nosound\*.* /n
rmdir %1\baseset\nosound

echo NoSound has been removed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.

:end
