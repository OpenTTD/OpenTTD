@echo off

set OPENTTD_VERSION=1.1.3
set OPENSFX_VERSION=0.8.0
set NOSOUND_VERSION=0.8.0
set OPENGFX_VERSION=0.7.0

echo To make the installer, you must have the WarpIN compiler (wic) installed and in
echo your path, as well as wget and unzip. This file will download the various DLLs
echo to be distributed with the installer. If you do not want to continue, please
echo press CTRL-C now.
echo.
pause

cd ..\..\..\bundle

if not exist SDL12.dll goto getsdl
if not exist FSLib.dll goto getsdl
goto libc

:getsdl

wget http://www.os2site.com/sw/dev/sdl/sdl-1.2.10-bin-20080804.zip -O dl.zip
unzip -j dl.zip SDL/FSLib.dll SDL/SDL12.dll
del dl.zip

:libc

if exist libc063.dll goto gcc

wget ftp://ftp.netlabs.org/pub/gcc/libc-0.6.3-csd3.zip -O dl.zip
unzip -j dl.zip libc063.dll
del dl.zip

:gcc

if exist gcc442.dll goto tools

wget http://www.owenrudge.net/various/gcc442.zip -O dl.zip
unzip -j dl.zip gcc442.dll
del dl.zip

:tools

cd ..\os\os2\installer
if exist tools goto opengfx

mkdir tools
cd tools

wget http://download.smedley.info/wget-1.11.4-os2-20090315.zip -O dl.zip
unzip -j dl.zip wget/bin/wget.exe
del dl.zip

wget ftp://ftp.os4.su/moveton/p7zip-9.04-bin-os2.zip -O dl.zip
unzip -j dl.zip bin/7za.exe dll/ilibca.dll
del dl.zip

cd ..

:opengfx

if exist opengfx goto opensfx

mkdir opengfx
sed s/OPENGFX_VERSION/%OPENGFX_VERSION%/ < download_opengfx.cmd > opengfx\download_opengfx.cmd
copy remove_opengfx.cmd opengfx

:opensfx
if exist opensfx goto nosound

mkdir opensfx
sed s/OPENSFX_VERSION/%OPENSFX_VERSION%/ < download_opensfx.cmd > opensfx\download_opensfx.cmd
copy remove_opensfx.cmd opensfx

:nosound

mkdir nosound
sed s/NOSOUND_VERSION/%NOSOUND_VERSION%/ < download_nosound.cmd > nosound\download_nosound.cmd
copy remove_nosound.cmd nosound

:end

if exist openttd-%OPENTTD_VERSION%-os2.exe del openttd-%OPENTTD_VERSION%-os2.exe
wic -a openttd-%OPENTTD_VERSION%-os2.exe 1 -c../../../bundle -r * 2 -ctools -r * 3 -copengfx -r * 4 -copensfx -r * 5 -cnosound -r * -U -s openttd.wis
