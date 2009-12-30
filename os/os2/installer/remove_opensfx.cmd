@echo off
if "%1" == "" goto err

echo Removing OpenSFX...

del %1\data\opensfx\*.* /n
rmdir %1\data\opensfx

echo OpenSFX has been removed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.

:end
