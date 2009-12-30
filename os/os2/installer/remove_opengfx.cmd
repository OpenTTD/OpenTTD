@echo off
if "%1" == "" goto err

echo Removing OpenGFX...

del %1\data\opengfx\*.* /n
rmdir %1\data\opengfx

echo OpenGFX has been removed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.

:end
