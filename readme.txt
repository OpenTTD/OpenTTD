OpenTTD README
Last updated:    2008-02-18
Release version: 0.6.0-beta4
------------------------------------------------------------------------


Table of Contents:
------------------
1.0) About
2.0) Contacting
 * 2.1 Reporting Bugs
3.0) Supported Platforms
4.0) Installing and running OpenTTD
 * 4.1 (Required) 3rd party files
 * 4.2 OpenTTD directories
 * 4.3 Portable Installations (External Media)
5.0) OpenTTD features
6.0) Configuration File
7.0) Compiling
8.0) Translating
 * 8.1 Guidelines
 * 8.2 Translation
 * 8.3 Previewing
9.0) Troubleshooting
X.X) Credits


1.0) About:
---- ------
OpenTTD is a clone of Transport Tycoon Deluxe, a popular game originally
written by Chris Sawyer.  It attempts to mimic the original game as closely
as possible while extending it with new features.

OpenTTD is licensed under the GNU General Public License version 2.0. For
more information, see the file 'COPYING'.

2.0) Contacting:
---- ----------
The easiest way to contact the OpenTTD team is by submitting bug reports or
posting comments in our forums. You can also chat with us on IRC (#openttd
on irc.oftc.net).

The OpenTTD homepage is http://www.openttd.org/.

You can also find the OpenTTD forums at
http://forum.openttd.org/


2.1) Reporting Bugs:
---- ---------------
To report a bug, please create a Flyspray account and follow the bugs
link from our homepage. Please make sure the bug is reproducible and
still occurs in the latest daily build or the current SVN version. Also
please look through the existing bug reports briefly to see whether the bug
is not already known.

The Flyspray project page URL is: http://bugs.openttd.org/

Please include the following information in your bug report:
        - OpenTTD version (PLEASE test the latest SVN/nightly build)
        - Bug details, including instructions how to reproduce it
        - Platform and compiler (Win32, Linux, FreeBSD, ...)
        - Attach a saved game *and* a screenshot if possible
        - If this bug only occurred recently please note the last
          version without the bug and the first version including
          the bug. That way we can fix it quicker by looking at the
          changes made.


3.0) Supported Platforms:
---- --------------------
OpenTTD has been ported to several platforms and operating systems. It shouldn't
be very difficult to port it to a new platform. The currently working platforms
are:

  BeOS                 - SDL
  FreeBSD              - SDL
  Linux                - SDL
  MacOS X (universal)  - Cocoa video and sound drivers (SDL works too, but not 100% and not as a universal binary)
  MorphOS              - SDL
  OpenBSD              - SDL
  OS/2                 - SDL
  Windows              - Win32 GDI (faster) or SDL


4.0) Installing and running OpenTTD:
---- -------------------------------

Installing OpenTTD is fairly straightforward. Either you have downloaded an
archive which you have to extract to a directory where you want OpenTTD to
be installed, or you have downloaded an installer, which will automatically
extract OpenTTD in the given directory.

OpenTTD looks in multiple locations to find the required data files (described
in section 4.2). Installing any 3rd party files into a "shared" location has
the advantage that you only need to do this step once, rather than copying the
data files into all OpenTTD versions you have.
Savegames, screenshots, etc are saved relative to the config file (openttd.cfg)
currently being used. This means that if you use a config file in one of the
shared directories, savegames will reside in the save/ directory next to the
openttd.cfg file there.
If you want savegames and screenshots in the directory where the OpenTTD binary
resides, simply have your config file in that location. But if you remove this
config file, savegames will still be in this directory (see notes in section 4.2)

4.1) (Required) 3rd party files:
---- ---------------------------

Before you run OpenTTD, you need to put the game's datafiles into a data/
directory which can be located in various places addressed in the following
section.
As OpenTTD makes use of the original TTD artwork you will need the files listed
below, which you can find on a Transport Tycoon Deluxe CD-ROM.
The Windows installer optionally can copy these files from that CD-ROM.

List of the required files:
	- sample.cat
	- trg1r.grf
	- trgcr.grf
	- trghr.grf
	- trgir.grf
	- trgtr.grf

Alternatively you can use the TTD GRF files from the DOS version:
	- TRG1.GRF
	- TRGC.GRF
	- TRGH.GRF
	- TRGI.GRF
	- TRGT.GRF

If you want the TTD music, copy the gm/ folder from the Windows version
of TTD to your OpenTTD folder (not your data folder - also explained in
the following sections).

Do NOT copy files included with OpenTTD into "shared" directories (explained in
the following sections) as sooner or later you will run into graphical glitches
when using other versions of the game.

4.2) OpenTTD directories
---- -------------------------------

The TTD artwork files listed in the section 4.1 "(Required) 3rd party files"
can be placed in a few different locations:
	1. The current working directory (from where you started OpenTTD)
	2. Your personal directory
		Windows: C:\Documents and Settings\<username>\My Documents\OpenTTD
		Mac OSX: ~/Documents/OpenTTD
		Linux:   ~/.openttd
	3. The shared directory
		Windows: C:\Documents and Settings\All Users\Documents\OpenTTD
		Mac OSX: /Library/Application Support/OpenTTD
		Linux:   not available
	4. The binary directory (where the OpenTTD executable is)
		Windows: C:\Program Files\OpenTTD
		Linux:   /usr/games
	5. The installation directory (Linux only)
		Linux:   /usr/share/games/openttd
	6. The application bundle (Mac OSX only)
		It includes the OTTD files (grf+lng) and it will work as long as they aren't touched

Notes:
	- Linux in the previous list means .deb, but most paths should be similar for others.
	- The previous search order is also used for newgrfs and openttd.cfg.
	- If openttd.cfg is not found, then it will be created using the 2, 4, 1, 3, 5 order.
	- Savegames will be relative to the config file only if there is no save/
	  directory in paths with higher priority than the config file path, but
	  autosaves and screenshots will always be relative to the config file.

The prefered setup:
Place 3rd party files in shared directory (or in personal directory if you don't
have write access on shared directory) and have your openttd.cfg config file in
personal directory (where the game will then also place savegames and screenshots).


4.3) Portable Installations (External Media):
---- ----------------------------------------

You can install OpenTTD on external media so you can take it with you, i.e.
using a USB key, or a USB HDD, etc.
Create a directory where you shall store the game in (i.e. OpenTTD/).
Copy the binary (OpenTTD.exe, OpenTTD.app, openttd, etc), data/ and your
openttd.cfg to this directory.
You can copy binaries for any operating system into this directory, which will
allow you to play the game on nearly any computer you can attach the external
media to.
As always - additional grf files are stored in the data/ dir (for details,
again, see section 4.1).


5.0) OpenTTD features:
---- -----------------

OpenTTD has a lot of features going beyond the original TTD emulation.
Unfortunately, there is currently no comprehensive list of features, but there
is a basic features list on the web, and some optional features can be
controlled through the Configure Patches dialog. We also implement some
features known from TTDPatch (http://www.ttdpatch.net/).

Several important non-standard controls:

* Use Ctrl to place semaphore signals
* Ingame console. More information at
  http://wiki.openttd.org/index.php/Console


6.0) Configuration File:
---- -------------------
The configuration file for OpenTTD (openttd.cfg) is in a simple Windows-like
.INI format. It's mostly undocumented. Almost all settings can be changed
ingame by using the 'Configure Patches' window.


7.0) Compiling:
---- ----------
Windows:
  You need Microsoft Visual Studio .NET. Open the project file
  and it should build automatically. In case you want to build with SDL support
  you need to add WITH_SDL to the project settings.
  PNG (WITH_PNG) and ZLIB (WITH_ZLIB) support is enabled by default. For these
  to work you need their development files. For best results, download the
  openttd-useful.zip file from SourceForge under the Files tab. Put the header
  files into your compiler's include/ directory and the library (.lib) files
  into the lib/ directory.
  For more help with VS see docs/Readme_Windows_MSVC.txt.

  You can also build it using the Makefile with MSYS/MinGW or Cygwin/MinGW.
  Please read the Makefile for more information.

Solaris 10:
  You need g++ (version 3 or higher), together with SDL. Installation of
  libpng and zlib is recommended. For the first build it is required
  to execute "bash configure" first. Note that ./configure does not work
  yet. It is likely that you don't have a strip binary, so use the
  --disable-strip option in that case. Fontconfig (>2.3.0) and freetype
  are optional. "make run" will then run the program.

Unix:
  OpenTTD can be built with GNU "make". On non-GNU systems it's called "gmake".
  However, for the first build one has to do a "./configure" first.
  Note that you need SDL-devel 1.2.5 (or higher) to compile OpenTTD.

MacOS X:
  Use "make" or Xcode (which will then call make for you)
  This will give you a binary for your CPU type (PPC/Intel)
  However, for the first build one has to do a "./configure" first.
  To make a universal binary type "./configure --enabled-universal"
  instead of "./configure".

BeOS:
  Use "make", but do a "./configure" before the first build.

FreeBSD:
  You need the port devel/sdl12 for a non-dedicated build.
  graphics/png is optional for screenshots in the PNG format.
  Use "gmake", but do a "./configure" before the first build.

OpenBSD:
  Use "gmake", but do a "./configure" before the first build.
  Note that you need the port devel/sdl to compile OpenTTD.

MorphOS:
  Use "make". However, for the first build one has to do a "./configure" first.
  Note that you need the MorphOS SDK, latest libnix updates (else C++ parts of
  OpenTTD will not build) and the powersdl.library SDK. Optionally libz,
  libpng and freetype2 developer files.

OS/2:
  A comprehensive GNU build environment is required to build the OS/2 version.
  See the docs/Readme_OS2.txt file for more information.


8.0) Translating:
---- -------------------
See http://www.openttd.org/translating.php for up-to-date information.

The use of the online Translator service, located at
http://translator2.openttd.org/, is highly encouraged. For a username/password
combo you should contact the development team, either by mail, IRC or the
forums. The system is straightforward to use, and if you have any problems,
read the online help located there.

If for some reason the website is down for a longer period of time, the
information below might be of help.

8.1) Guidelines:
---- -------------------
Here are some translation guidelines which you should follow closely.

    * Please contact the development team before beginning the translation
      process! This avoids double work, as someone else may have already
      started translating to the same language.

8.2) Translation:
---- -------------------
So, now that you've notified the development team about your intention to
translate (You did, right? Of course you did.) you can pick up english.txt
(found in the SVN repository under /lang) and translate.

You must change the first two lines of the file appropriately:

##name English-Name-Of-Language
##ownname Native-Name-Of-Language

Note: Do not alter the following parts of the file:

    * String identifiers (the first word on each line)
    * Parts of the strings which are in curly braces (such as {STRING})
    * Lines beginning with ## (such as ##id), other than the first two lines of
      the file

8.3) Previewing:
---- -------------------
In order to view the translation in the game, you need to compile your language
file with the strgen utility, which is now bundled with the game.

strgen is a command-line utility. It takes the language filename as parameter.
Example:

strgen lang/german.txt

This results in compiling german.txt and produces another file named german.lng.
Any missing strings are replaced with the English strings. Note that it looks
for english.txt in the lang subdirectory, which is where your language file
should also be.

That's all! You should now be able to select the language in the game options.

9.0) Troubleshooting
---- ---------------

To see all startup options available to you, start OpenTTD with the
"./openttd -h" option. This might help you tweak some of the settings.

If the game is acting strange and you feel adventurous you can try the
"-d [[<name>]=[<level>]" flag, where the higher levels will give you more
debugging output. The "name" variable can help you to display only some type of
debugging messages. This is mostly undocumented so best is to look in the
source code file debug.c for the various debugging types. For more information
look at http://wiki.openttd.org/index.php/Command_line.

The most frequent problem is missing data files. Don't forget to put all GRF
files from TTD into your data/ folder including sample.cat!

Under Windows 98 and lower it is impossible to use a dedicated server; it will
fail to start. Perhaps this is for the better because those OS's are not known
for their stability.

With the added support for font-based text selecting a non-latin language will
result in garbage (lots of '?') shown on screen. Please open your configuration
file and add a desired font for small/medium/-and large_font. This can be a font
name like "Tahoma" or a path to a font.

Any NewGRF file used in a game is stored inside the savegame and will refuse
to load if you don't have that grf file available. A list of missing files
will be output to the console at the moment, so use the '-d' flag (on windows)
to see this list. You just have to find the files (http://grfcrawler.tt-forums.net/)
put them in the data/ folder and you're set to go.

X.X) Credits:
---- --------
The OpenTTD team (in alphabetical order):
  Jean-Francois Claeys (Belugas) - GUI, newindustries and more
  Bjarni Corfitzen (Bjarni)      - MacOSX port, coder and vehicles
  Matthijs Kooijman (blathijs)   - Pathfinder-guru, pool rework
  Loïc Guilloux (glx)            - General coding
  Christoph Elsenhans (frosch)   - General coding
  Jaroslav Mazanec (KUDr)        - YAPG (Yet Another Pathfinder God) ;)
  Jonathan Coome (Maedhros)      - High priest of the newGRF Temple
  Attila Bán (MiHaMiX)           - WebTranslator, Nightlies, Wiki and bugtracker host
  Owen Rudge (orudge)            - Forum host, OS/2 port
  Peter Nelson (peter1138)       - Spiritual descendant from newGRF gods
  Remko Bijker (Rubidium)        - Lead coder and way more
  Benedikt Brüggemeier (skidd13) - Bug fixer and code reworker
  Zdenek Sojka (SmatZ)           - Bug finder and fixer

Inactive Developers:
  Victor Fischer (Celestar)      - Programming everywhere you need him to
  Tamás Faragó (Darkvater)       - Ex-Lead coder
  Christoph Mallon (Tron)        - Programmer, code correctness police

Retired Developers:
  Ludvig Strigeus (ludde)        - OpenTTD author, main coder (0.1 - 0.3.3)
  Serge Paquet (vurlix)          - Assistant project manager, coder (0.1 - 0.3.3)
  Dominik Scherer (dominik81)    - Lead programmer, GUI expert (0.3.0 - 0.3.6)
  Patric Stout (TrueLight)       - Programmer, webhoster (0.3 - pre0.6)

Thanks to:
  Josef Drexler                  - For his great work on TTDPatch.
  Marcin Grzegorczyk             - For his TTDPatch work and documentation of TTD internals and graphics (signals and track foundations)
  Petr Baudis (pasky)            - Many patches, newgrf support, etc.
  Simon Sasburg (HackyKid)       - For the many bugfixes he has blessed us with
  Stefan Meißner (sign_de)       - For his work on the console
  Mike Ragsdale                  - OpenTTD installer
  Cian Duffy (MYOB)              - BeOS port / manual writing
  Christian Rosentreter (tokai)  - MorphOS / AmigaOS port
  Richard Kempton (RichK67)      - Additional airports, initial TGP implementation
  Michael Blunck                 - For revolutionizing TTD with awesome graphics
  George                         - Canal graphics
  David Dallaston (Pikka)        - Tram tracks
  All Translators                - For their support to make OpenTTD a truly international game
  Bug Reporters                  - Thanks for all bug reports
  Chris Sawyer                   - For an amazing game!
