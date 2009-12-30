@echo off
if "%1" == "" goto err

echo Removing NoSound...

del %1\data\nosound\*.* /n
rmdir %1\data\nosound

echo NoSound has been removed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.

:end
