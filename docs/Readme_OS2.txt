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
http://sourceforge.net/projects/openttd/ - see "os2-useful-v1.1.zip".
Version 20051222 of SDL or later is required. This can be found at
http://sdl.netlabs.org/.

Please note that earlier SDL releases will probably NOT work with
OpenTTD. If you experience problems with OpenTTD, please check
your SDL and FSLib.dll versions (both must match).

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

Please note also that the GCC version does not currently support the MCI MIDI
system.


A NOTE ABOUT DEDICATED MULTIPLAYER SERVERS
------------------------------------------

To start a dedicated multiplayer server, you should run the dedicated.cmd
file. This enables OpenTTD to open up a VIO console window to display
its output and gather any necessary input. Running "openttd -D"
directly will result in the console not being displayed. You may
still pass any other parameters ('-D' is already passed) to
dedicated.cmd.

You can find the dedicated.cmd file in the os/os2 directory.

=========================
BUILDING THE OS/2 VERSION
=========================

Compiler
--------

Innotek GCC, an OS/2 port of the popular GCC compiler, was used to build OpenTTD.
See www.innotek.de for more information. You WILL need a reasonably UNIX-like
build environment in order to build OpenTTD successfully - the following link
may help to set one up (although some of the links from that page are broken):

   http://www.mozilla.org/ports/os2/gccsetup.html

Alternatively, Paul Smedley's ready-to-go GCC build environment has been known to
successfully build the game:

   http://www.smedley.info/os2ports/index.php?page=build-environment

To build, you should, if your environment is set up well enough, be able to just
type `./configure' (or `sh configure' if you're using the OS/2 shell) and `make'.

You may have to manually specify `--os OS2' on the configure command line, as
configure cannot always detect OS/2 correctly.

A note on Open Watcom
---------------------

Open Watcom C/C++ was previously used to build OpenTTD (version 0.4.x and earlier).
However, due to advanced C++ features used in the YAPF portion of OpenTTD 0.5
in particular, the compiler is no longer able to build the game at the moment.
Hopefully one day Open Watcom will be able to catch up and we will be able to build
the game once again (it's easier than getting an OS/2 UNIX-like environment set up
in my opinion!), but until then, OpenTTD 0.5 and later can only be built with GCC.

Libraries Required
------------------

The following libraries are required. To build zlib and libpng, I
simply added the required files (watch out for sample programs, etc)
to an IDE project file and built a library. Do not use the makefiles
provided, they are not designed for Watcom (apart from SDL):

- zlib
  http://www.zlib.org/

- libpng
  http://www.libpng.org/

- SDL for OS/2
  ftp://ftp.netlabs.org/pub/sdl/sdl-1.2.7-src-20051222.zip used for
  0.4.7

- Freetype
  http://freetype.sourceforge.net/

Currently, there are no pre-built libraries available for GCC. If you manage to get
OpenTTD working on Watcom though (do let us know if this is the case!), pre-built
versions can be downloaded from the Files section at
http://sourceforge.net/projects/openttd/ - see "os2-useful-v1.1.zip".

Contact Information
-------------------

If you have any questions regarding OS/2 issues, please contact me
(owen@owenrudge.net) and I'll try to help you out. For general OpenTTD
issues, see the Contacting section of readme.txt.

Thanks to Paul Smedley for his help with getting OpenTTD to compile under GCC on OS/2.

- Owen Rudge, 24th June 2007
