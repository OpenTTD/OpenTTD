@echo off
if "%1" == "" goto err
if "%2" == "" goto err

echo Downloading OpenSFX...

%1\wget http://binaries.openttd.org/installer/opensfx-OPENSFX_VERSION.7z -O %2/data/opensfx.7z

echo Extracting OpenSFX...

%1\7za x -y -O%2/data %2/data/opensfx.7z
del %1\data\opensfx.7z /n

echo OpenSFX has been installed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.
echo Please visit www.openttd.org for details on downloading OpenSFX.

:end
