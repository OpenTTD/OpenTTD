OpenTTD: OS/2 version
=====================

OpenTTD has been ported to work on OS/2 4.x or later (including
eComStation). The game should work as well as it does on Windows
or other platforms: the main issues you may encounter are graphics
card problems, but that is really the fault of SDL.

=========================
USING OPENTTD FOR OS/2
=========================

LIBRARIES REQUIRED FOR END USERS
--------------------------------

SDL.DLL (SDL 1.2.7) and FSLib.dll are required to use this program:
these can be downloaded from the Files section at
http://sourceforge.net/projects/openttd/ - see "os2-useful.zip".
(Note that a newer version of SDL is now available at
ftp://ftp.netlabs.org/pub/sdl/sdl-dev-os2-2004-12-22.zip which may
help solve some problems).

Note that to actually play the game, I have found in my own
experience that a version of the Scitech Display Drivers or its later
incarnation (see www.scitech.com) are necessary for it to work. If
you have trouble with your native drivers, try the Scitech drivers
and see if they help the problem.

KNOWN ISSUES
------------

- If an error occurs during loading, the OS/2 error message window
  is not always displayed.

A NOTE ABOUT MUSIC
------------------

OpenTTD includes a music driver which uses the MCI MIDI system. Unfortunately,
due to the lack of proper MIDI hardware myself, I have been unable to test it,
but during testing, I found that when MIDI was enabled, I got no sound
effects. I therefore decided to DISABLE music by default.

To enable music, start OpenTTD with the command line:

   openttd -m os2

If I hear enough responses that both music and sound work together (it might
just be my system), I'll have the defaults changed.

A NOTE ABOUT DEDICATED MULTIPLAYER SERVERS
------------------------------------------

To start a dedicated multiplayer server, you should run the dedicated.cmd
file. This enables OpenTTD to open up a VIO console window to display
its output and gather any necessary input. Running "openttd -D"
directly will result in the console not being displayed. You may
still pass any other parameters ('-D' is already passed) to
dedicated.cmd.

=========================
BUILDING THE OS/2 VERSION
=========================

Compiler
--------

Open Watcom 1.3 was used to build OpenTTD (earlier versions will
NOT work). See http://www.openwatcom.org/ to download it. It may
also be possible to build OpenTTD under OS/2: I attempted this
before using Open Watcom, but found the tools available for OS/2
at the time to be a bit more tricky to get working.

Due to complexities in my set-up, I actually used the Win32 version
of Open Watcom to initially compile OpenTTD for OS/2. There should
be no reason of course why the OS/2 version cannot be used.

Libraries Required
------------------

The following libraries are required. To build zlib and libpng, I
simply added the required files (watch out for sample programs, etc)
to an IDE project file and built a library. Do not use the makefiles
provided, they are not designed for Watcom (apart from SDL):

- zlib
  http://www.zlib.org/ - contains a makefile for OS/2, but is out
  of date and uses EMX

- libpng
  http://www.libpng.org/ - contains an EMX/gcc makefile

- SDL for OS/2
  For 0.3.5, I used ftp://ftp.netlabs.org/pub/sdl/SDL-1.2.7-src-20040908a.zip -
  take SDL.dll and SDL.lib from the src/ directory. Note that 20041222 is
  out now, which is recommended for stability updates.

If you do not wish to build the libraries yourself, pre-built versions
can be downloaded from the Files section at
http://sourceforge.net/projects/openttd/ - see "os2-useful.zip".

Compiling
---------

To compile, open the os/os2/openttd.wpj file in the IDE and first build
the strgen.exe target. This will build the .lng file generator, and will
also attempt to build all the language files (plus the table\strings.h
file which is required for openttd.exe to be built). Once strgen.exe and
the language files are built successfully, you can build the openttd.exe
target.

Contact Information
-------------------

If you have any questions regarding OS/2 issues, please contact me
(owen@owenrudge.net) and I'll try to help you out. For general OpenTTD
issues, see the Contacting section of readme.txt.

- Owen Rudge, 26th December 2004