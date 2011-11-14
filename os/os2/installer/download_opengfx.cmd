@echo off
if "%1" == "" goto err
if "%2" == "" goto err

echo Downloading OpenGFX...

%1\wget http://binaries.openttd.org/installer/opengfx-OPENGFX_VERSION.7z -O %2/baseset/opengfx.7z

echo Extracting OpenGFX...

%1\7za x -y -O%2/baseset %2/baseset/opengfx.7z
del %1\baseset\opengfx.7z /n

echo OpenGFX has been installed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.
echo Please visit www.openttd.org for details on downloading OpenGFX.

:end
