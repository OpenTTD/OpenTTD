@echo off
if "%1" == "" goto err

echo Removing OpenSFX...

del %1\baseset\opensfx\*.* /n
rmdir %1\baseset\opensfx

echo OpenSFX has been removed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.

:end
