OpenTTD: OS/2 version  ** CURRENTLY INCOMPLETE **
=====================

OpenTTD has been ported to work on OS/2 4.x or later (including eComStation). At the moment, it does not work properly, but it
can compile and work to an extent.

Compiler
--------

Open Watcom 1.3 was used to build OpenTTD (earlier versions will NOT work). See http://www.openwatcom.org/ to download it.
It may also be possible to build OpenTTD under OS/2: I attempted this before using Open Watcom, but found the tools available
for OS/2 at the time to be a bit more tricky to get working.

Due to complexities in my set-up, I actually used the Win32 version of Open Watcom to initially compile OpenTTD for OS/2. There
should be no reason of course why the OS/2 version cannot be used.

Libraries Required
------------------

The following libraries are required. To build zlib and libpng, I simply added the required files (watch out for sample
programs, etc) to an IDE project file and built a library:

- zlib
  http://www.zlib.org/ - contains a makefile for OS/2, but is out of date and uses EMX

- libpng
  http://www.libpng.org/ - contains an EMX/gcc makefile

- SDL for OS/2
  I used ftp://ftp.netlabs.org/pub/sdl/SDL-1.2.7-src-20040908a.zip - take SDL.dll and SDL.lib from the src/ directory.

Note that to use the compiled program, you also need FSLib.dll (from src/ in the SDL zip) and a version of the Scitech
Display Drivers or its later incarnation (see www.scitech.com).

Compiling
---------

To compile, open the os/os2/openttd.wpj file in the IDE and build the openttd.exe target.

TODO: compilation of language files properly


** THESE DOCS ARE INCOMPLETE FOR THE MOMENT, WILL BE COMPLETED SOON **

If you have any questions, please contact me (owen@owenrudge.net) and I'll try to help you out

- Owen Rudge, 18th December 2004

OpenTTD: OS/2 version  ** CURRENTLY INCOMPLETE **
=====================

OpenTTD has been ported to work on OS/2 4.x or later (including eComStation). At the moment, it does not work properly, but it
can compile and work to an extent.

Compiler
--------

Open Watcom 1.3 was used to build OpenTTD (earlier versions will NOT work). See http://www.openwatcom.org/ to download it.
It may also be possible to build OpenTTD under OS/2: I attempted this before using Open Watcom, but found the tools available
for OS/2 at the time to be a bit more tricky to get working.

Due to complexities in my set-up, I actually used the Win32 version of Open Watcom to initially compile OpenTTD for OS/2. There
should be no reason of course why the OS/2 version cannot be used.

Libraries Required
------------------

The following libraries are required. To build zlib and libpng, I simply added the required files (watch out for sample
programs, etc) to an IDE project file and built a library:

- zlib
  http://www.zlib.org/ - contains a makefile for OS/2, but is out of date and uses EMX

- libpng
  http://www.libpng.org/ - contains an EMX/gcc makefile

- SDL for OS/2
  I used ftp://ftp.netlabs.org/pub/sdl/SDL-1.2.7-src-20040908a.zip - take SDL.dll and SDL.lib from the src/ directory.

Note that to use the compiled program, you also need FSLib.dll (from src/ in the SDL zip) and a version of the Scitech
Display Drivers or its later incarnation (see www.scitech.com).

Compiling
---------

To compile, open the os/os2/openttd.wpj file in the IDE and build the openttd.exe target.

TODO: compilation of language files properly


** THESE DOCS ARE INCOMPLETE FOR THE MOMENT, WILL BE COMPLETED SOON **

If you have any questions, please contact me (owen@owenrudge.net) and I'll try to help you out

- Owen Rudge, 18th December 2004

