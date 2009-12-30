@echo off
if "%1" == "" goto err
if "%2" == "" goto err

echo Downloading NoSound...

%1\wget http://binaries.openttd.org/installer/nosound-NOSOUND_VERSION.7z -O %2/data/nosound.7z

echo Extracting NoSound...

%1\7za x -y -O%2/data %2/data/nosound.7z
del %1\data\nosound.7z /n

echo NoSound has been installed.
goto end

:err
echo This batch file is only intended for use by the OpenTTD installer.
echo Please visit www.openttd.org for details on downloading NoSound.

:end