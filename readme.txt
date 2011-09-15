OpenTTD readme
Last updated:    2011-09-15
Release version: 1.1.3
------------------------------------------------------------------------


Table of contents
-----------------
1.0) About
2.0) Contacting
 * 2.1) Reporting bugs
 * 2.2) Reporting desyncs
3.0) Supported platforms
4.0) Installing and running OpenTTD
 * 4.1) (Required) 3rd party files
 * 4.2) OpenTTD directories
 * 4.3) Portable installations (portable media)
5.0) OpenTTD features
 * 5.1) Logging of potentially dangerous actions
6.0) Configuration file
7.0) Compiling
 * 7.1) Required/optional libraries
 * 7.2) Supported compilers
8.0) Translating
 * 8.1) Translation
 * 8.2) Previewing
9.0) Troubleshooting
X.X) Credits


1.0) About
---- -----
OpenTTD is a transport simulation game based upon the popular game Transport
Tycoon Deluxe, written by Chris Sawyer. It attempts to mimic the original
game as closely as possible while extending it with new features.

OpenTTD is licensed under the GNU General Public License version 2.0. For
more information, see the file 'COPYING'.


2.0) Contacting
---- ----------
The easiest way to contact the OpenTTD team is by submitting bug reports or
posting comments in our forums. You can also chat with us on IRC (#openttd
on irc.oftc.net).

The OpenTTD homepage is http://www.openttd.org/.

You can also find the OpenTTD forums at
http://forum.openttd.org/

2.1) Reporting bugs
---- --------------
First of all, check whether the bug is not already known. Do this by looking
through the file called 'known-bugs.txt' which is distributed with OpenTTD
like this readme.

For tracking our bugs we are using a bug tracker called Flyspray. You can find
the tracker at http://bugs.openttd.org/. Before actually reporting take a look
through the already reported bugs there to see if the bug is already known.
The 'known-bugs.txt' file might be a bit outdated at the moment you are
reading it as only bugs known before the release are documented there. Also
look through the recently closed bugs.

When you are sure it is not already reported you should:
 * Make sure you are running a recent version, i.e. run the latest stable or
   nightly based on where you found the bug.
 * Make sure you are not running a non-official binary, like a patch pack.
   When you are playing with a patch pack you should report any bugs to the
   forum thread related to that patch pack.
 * Make it reproducible for the developers. In other words, create a savegame
   in which you can reproduce the issue once loaded. It is very useful to give
   us the crash.dmp, crash.sav, crash.log and crash screenshot which are
   created on crashes.
 * Check whether the bug is already reported on our bug tracker. This includes
   searching for recently closed bug reports as the bug might already be fixed.

After you have done all that you can report the bug. Please include the
following information in your bug report:
 * OpenTTD version (PLEASE test the latest SVN/nightly build)
 * Bug details, including instructions how to reproduce it
 * Platform (Win32, Linux, FreeBSD, ...) and compiler (including version) if
   you compiled OpenTTD yourself.
 * Attach a saved game *and* a screenshot if possible
 * If this bug only occurred recently please note the last version without
   the bug and the first version including the bug. That way we can fix it
   quicker by looking at the changes made.
 * Attach crash.dmp, crash.log and crash.sav. These files are usually created
   next to your openttd.cfg. The crash handler will tell you the location.

2.2) Reporting desyncs
---- -----------------
As desyncs are hard to make reproducible OpenTTD has the ability to log all
actions done by clients so we can replay the whole game in an effort to make
desyncs better reproducible. You need to turn this ability on. When turned
on an automatic savegame will be made once the map has been constructed in
the 'save/autosave' directory, see OpenTTD directories to know where to find
this directory. Furthermore the log file 'commands-out.log' will be created
and all actions will be written to there.

To enable the desync debugging you need to set the debug level for 'desync'
to at least 1. You do this by starting OpenTTD with '-d desync=<level>' as
parameter or by typing 'debug_level desync=<level>' in OpenTTD's internal
console.
The desync debug levels are:
 0: nothing.
 1: dumping of commands to 'commands-out.log'.
 2: same as 1 plus checking vehicle caches and dumping that too.
 3: same as 2 plus monthly saves in autosave.
 4 and higher: same as 3

Restarting OpenTTD will overwrite 'commands-out.log'. OpenTTD will not remove
the savegames (dmp_cmds_*.sav) made by the desync debugging system, so you
have to occasionally remove them yourself!

The naming format of the desync savegames is as follows:
dmp_cmds_XXXXXXXX_YYYYYYYY.sav. The XXXXXXXX is the hexadecimal representation
of the generation seed of the game and YYYYYYYY is the hexadecimal
representation of the date of the game. This sorts the savegames by game and
then by date making it easier to find the right savegames.

When a desync has occurred with the desync debugging turned on you should file
a bug report with the following files attached:
 - commands-out.log as it contains all the commands that were done
 - the last saved savegame (search for the last line beginning with
   'save: dmp_cmds_' in commands-out.log). We use this savegame to check
   whether we can quickly reproduce the desync. Otherwise we will need...
 - the first saved savegame (search for the first line beginning with 'save'
   where the first part, up to the last underscore '_', is the same). We need
   this savegame to be able to reproduce the bug when the last savegame is not
   old enough. If you loaded a scenario or savegame you need to attach that.
 - optionally you can attach the savegames from around 50%, 75%, 85%, 90% and
   95% of the game's progression. We can use these savegames to speed up the
   reproduction of the desync, but we should be able to reproduce these
   savegames based on the first savegame and commands-out.log.
 - in case you use any NewGRFs you should attach the ones you used unless
   we can easily find them ourselves via bananas or when they are in the
   #openttdcoop pack.

Do NOT remove the dmp_cmds savegames of a desync you have reported until the
desync has been fixed; if you, by accident, send us the wrong savegames we
will not be able to reproduce the desync and thus will be unable to fix it.


3.0) Supported platforms
---- -------------------
OpenTTD has been ported to several platforms and operating systems. It shouldn't
be very difficult to port it to a new platform. The currently working platforms
are:

  BeOS                 - SDL or Allegro
  DOS                  - Allegro
  FreeBSD              - SDL
  Linux                - SDL or Allegro
  MacOS X (universal)  - Cocoa video and sound drivers
  MorphOS              - SDL
  OpenBSD              - SDL
  OS/2                 - SDL
  Windows              - Win32 GDI (faster) or SDL or Allegro


4.0) Installing and running OpenTTD
---- ------------------------------
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

OpenTTD comes without AIs, so if you want to play with AIs you have to download
them. The easiest way is via the "Check Online Content" button in the main menu.
You can select some AIs that you think are compatible with your playing style.
Another way is manually downloading the AIs from the forum although then you
need to make sure that you install all the required AI libraries too; they get
automatically selected (and downloaded) if you get the AIs via the "Check
Online Content". If you do not have an AI but have configured OpenTTD to start
an AI a message will be shown that the 'dummy' AI has been started.

4.1) (Required) 3rd party files
---- --------------------------
Before you run OpenTTD, you need to put the game's data files into a data/
directory which can be located in various places addressed in the following
section.

For OpenTTD you need to acquire some third party data files. For this you have
the choice of using the original Transport Tycoon Deluxe data files or a set
of free data files.

Do NOT copy files included with OpenTTD into "shared" directories (explained in
the following sections) as sooner or later you will run into graphical glitches
when using other versions of the game.

4.1.1) Free graphics and sound files
------ -----------------------------
The free data files, split into OpenGFX for graphics, OpenSFX for sounds and
OpenMSX for music can be found at:
 - http://www.openttd.org/download-opengfx for OpenGFX
 - http://www.openttd.org/download-opensfx for OpenSFX
 - http://www.openttd.org/download-openmsx for OpenMSX
Please follow the readme of these packages about the installation procedure.
The Windows installer can optionally download and install these packages.

4.1.2) Original Transport Tycoon Deluxe graphics and sound files
------ ---------------------------------------------------------
If you want to play with the original Transport Tycoon Deluxe data files you
have to copy the data files from the CD-ROM into the data/ directory. It does
not matter whether you copy them from the DOS or Windows version of Transport
Tycoon Deluxe. The Windows install can optionally copy these files.
You need to copy the following files:
 - sample.cat
 - trg1r.grf or TRG1.GRF
 - trgcr.grf or TRGC.GRF
 - trghr.grf or TRGH.GRF
 - trgir.grf or TRGI.GRF
 - trgtr.grf or TRGT.GRF

4.1.3) Original Transport Tycoon Deluxe music
------ --------------------------------------
If you want the Transport Tycoon Deluxe music, copy the gm/ folder from the
Windows version of Transport Tycoon Deluxe to your OpenTTD folder (not your
data folder - also explained in the following sections). The music from the
DOS version as well as the original Transport Tycoon does not work.

4.1.4) AIs
------ ---
If you want AIs use the in-game content downloader. If for some reason that is
not possible or you want to use an AI that has not been uploaded to the content
download system download the tar file and place it in the ai/ directory. If the
AI needs libraries you'll have to download those too and put them in the
ai/library/ directory. All AIs and AI Libraries that have been uploaded to
the content download system can be found at http://noai.openttd.org/downloads/
The AIs and libraries can be found their in the form of .tar.gz packages.
OpenTTD can read inside tar files but it does not extract .tar.gz files by
itself.
To figure out which libraries you need for an AI you have to start the AI and
wait for an error message to pop up. The error message will tell you
"couldn't find library 'lib-name'". Download that library and try again.

4.2) OpenTTD directories
---- -------------------
OpenTTD uses its own directory to store its required 3rd party base set files (see section
4.1 "Required 3rd party files") and non-compulsory extension and configuration files. See
below for their proper place within this OpenTTD main data directory.

The main OpenTTD directories can be found in various locations, depending on your operating
system:
	1. The current working directory (from where you started OpenTTD)
		For non-Windows operating systems OpenTTD will not scan for files in this
		directory if it is your personal directory, i.e. "~/", or when it is the
		root directory, i.e. "/".
	2. Your personal directory
		Windows: C:\My Documents (95, 98, ME)
		         C:\Documents and Settings\<username>\My Documents\OpenTTD (2000, XP)
		         C:\Users\<username>\Documents\OpenTTD (Vista, 7)
		Mac OSX: ~/Documents/OpenTTD
		Linux:   ~/.openttd
	3. The shared directory
		Windows: C:\Documents and Settings\All Users\Shared Documents\OpenTTD (2000, XP)
		         C:\Users\Public\Documents\OpenTTD (Vista, 7)
		Mac OSX: /Library/Application Support/OpenTTD
		Linux:   not available
	4. The binary directory (where the OpenTTD executable is)
		Windows: C:\Program Files\OpenTTD
		Linux:   /usr/games
	5. The installation directory (Linux only)
		Linux:   /usr/share/games/openttd
	6. The application bundle (Mac OSX only)
		It includes the OpenTTD files (grf+lng) and it will work as long as they aren't
		touched

Different types of data or extensions go into different subdirectories of the chosen main
OpenTTD directory:
	Config File:         (no subdirectory)
	Screenshots:         (no subdirectory)
	Base Graphics:       data                    (or a subdirectory thereof)
	Sound Sets:          data                    (or a subdirectory thereof)
	NewGRFs:             data                    (or a subdirectory thereof)
	32bpp Sets:          data                    (or a subdirectory thereof)
	Music Sets:          gm                      (or a subdirectory thereof)
	AIs:                 ai                      (or a subdirectory thereof)
	AI Libraries:        ai/libraries            (or a subdirectory thereof)
	Savegames:           save
	Automatic Savegames: save/autosave
	Scenarios:           scenario

The (automatically created) directory content_download is for OpenTTD's internal use and
no files should be added to it or its subdirectories manually.

Notes:
	- Linux in the previous list means .deb, but most paths should be similar for others.
	- The previous search order is also used for NewGRFs and openttd.cfg.
	- If openttd.cfg is not found, then it will be created using the 2, 4, 1, 3, 5 order.
	- Savegames will be relative to the config file only if there is no save/
	  directory in paths with higher priority than the config file path, but
	  autosaves and screenshots will always be relative to the config file.

The preferred setup:
Place 3rd party files in shared directory (or in personal directory if you don't
have write access on shared directory) and have your openttd.cfg config file in
personal directory (where the game will then also place savegames and screenshots).

4.3) Portable installations (portable media)
---- ---------------------------------------
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


5.0) OpenTTD features
---- ----------------
OpenTTD has a lot of features going beyond the original Transport Tycoon Deluxe
emulation. Unfortunately, there is currently no comprehensive list of features,
but there is a basic features list on the web, and some optional features can be
controlled through the Advanced Settings dialog. We also implement some
features known from TTDPatch (http://www.ttdpatch.net/).

Several important non-standard controls:

* Ctrl modifies many commands and makes them more powerful. For example Ctrl
  clicking on signals with the build signal tool changes their behaviour, holding
  Ctrl while the track build tool is activated changes it to the track removal
  tool, and so on. See http://wiki.openttd.org/Hidden_features for a non-
  comprehensive list or look at the tooltips.
* Ingame console. More information at
  http://wiki.openttd.org/index.php/Console
* Hovering over a GUI element shows tooltips. This can be changed to right click
  via the advanced settings.

5.1) Logging of potentially dangerous actions
---- ----------------------------------------
OpenTTD is a complex program, and together with NewGRF, it may show a buggy
behaviour. But not only bugs in code can cause problems. There are several
ways to affect game state possibly resulting in program crash or multiplayer
desyncs.
Easier way would be to forbid all these unsafe actions, but that would affect
game usability for many players. We certainly do not want that.
However, we receive bug reports because of this. To reduce time spent with
solving these problems, these potentially unsafe actions are logged in
the savegame (including crash.sav). Log is stored in crash logs, too.

Information logged:

* Adding / removing / changing order of NewGRFs
* Changing NewGRF parameters, loading compatible NewGRF
* Changing game mode (scenario editor <-> normal game)
* Loading game saved in a different OpenTTD / TTDPatch / Transport Tycoon Deluxe /
  original Transport Tycoon version
* Running a modified OpenTTD build
* Changing settings affecting NewGRF behaviour (non-network-safe settings)
* Changing landscape (by cheat)
* Triggering NewGRF bugs

No personal information is stored.

You can show the game log by typing 'gamelog' in the console or by running
OpenTTD in debug mode.


6.0) Configuration file
---- ------------------
The configuration file for OpenTTD (openttd.cfg) is in a simple Windows-like
.INI format. It's mostly undocumented. Almost all settings can be changed
ingame by using the 'Advanced Settings' window.
When you cannot find openttd.cfg you should look in the directories as
described in section 4.2. If you do not have an openttd.cfg OpenTTD will
create one after closing.


7.0) Compiling
---- ---------
Windows:
  You need Microsoft Visual Studio .NET. Open the project file
  and it should build automatically. In case you want to build with SDL support
  you need to add WITH_SDL to the project settings.
  PNG (WITH_PNG) and ZLIB (WITH_ZLIB) support is enabled by default. For these
  to work you need their development files. For best results, download the
  openttd-useful.zip file from http://www.openttd.org/download-openttd-useful
  Put the header files into your compiler's include/ directory and the
  library (.lib) files into the lib/ directory.
  For more help with VS see docs/Readme_Windows_MSVC.txt.

  You can also build it using the Makefile with MSYS/MinGW or Cygwin/MinGW.
  Please read the Makefile for more information.

Solaris, FreeBSD, OpenBSD:
  Use "gmake", but do a "./configure" before the first build.

Linux/Unix:
  OpenTTD can be built with GNU "make". On non-GNU systems it's called "gmake".
  However, for the first build one has to do a "./configure" first.

MacOS X:
  Use "make" or Xcode (which will then call make for you)
  This will give you a binary for your CPU type (PPC/Intel)
  However, for the first build one has to do a "./configure" first.
  To make a universal binary type "./configure --enabled-universal"
  instead of "./configure".

BeOS:
  Use "make", but do a "./configure" before the first build.

MorphOS:
  Use "make". However, for the first build one has to do a "./configure" first.
  Note that you need the MorphOS SDK, latest libnix updates (else C++ parts of
  OpenTTD will not build) and the powersdl.library SDK. Optionally libz,
  libpng and freetype2 developer files.

OS/2:
  A comprehensive GNU build environment is required to build the OS/2 version.
  See the docs/Readme_OS2.txt file for more information.

DOS:
  A build environment with DJGPP is needed as well as libraries such as
  Allegro, zlib and libpng, which all can be downloaded from the DJGPP
  website. Compilation is straight forward: use make, but do a "./configure"
  before the first build. The build binary will need cwsdpmi.exe to be in
  the same directory as the openttd executable. cwsdpmi.exe can be found in
  the os/dos subdirectory. If you compile with stripping turned on a binary
  will be generated that does not need cwsdpmi.exe by adding the cswdstub.exe
  to the created OpenTTD binary.

7.1) Required/optional libraries
---- ---------------------------
The following libraries are used by OpenTTD for:
  - libSDL/liballegro: hardware access (video, sound, mouse)
  - zlib: (de)compressing of old (0.3.0-1.0.5) savegames, content downloads,
    heightmaps
  - liblzo2: (de)compressing of old (pre 0.3.0) savegames
  - liblzma: (de)compressing of savegames (1.1.0 and later)
  - libpng: making screenshots and loading heightmaps
  - libfreetype: loading generic fonts and rendering them
  - libfontconfig: searching for fonts, resolving font names to actual fonts
  - libicu: handling of right-to-left scripts (e.g. Arabic and Persian) and
    natural sorting of strings.

OpenTTD does not require any of the libraries to be present, but without
liblzma you cannot open most recent savegames and without zlib you cannot
open most older savegames or use the content downloading system.
Without libSDL/liballegro on non-Windows and non-MacOS X machines you have
no graphical user interface; you would be building a dedicated server.

To recompile the extra graphics needed to play with the original Transport
Tycoon Deluxe graphics you need GRFCodec (which includes NFORenum) as well.
GRFCodec can be found at: http://www.openttd.org/download-grfcodec
The compilation of these extra graphics does generally not happen, unless
you remove the graphics file using "make maintainer-clean".

7.2) Supported compilers
---- -------------------
The following compilers are known to compile OpenTTD:
  - Microsoft Visual C++ (MSVC) 2005, 2008 and 2010.
    Version 2005 gives bogus warnings about scoping issues.
  - GNU Compiler Collection (GCC) 3.3 - 4.7.
    Versions 4.1 and earlier give bogus warnings about uninitialised variables.
    Versions 4.4 - 4.6 give bogus warnings about freeing non-heap objects.
    Versions 4.5 and later give invalid warnings when lto is enabled.
  - Intel C++ Compiler (ICC) 12.0.

The following compilers are known not to compile OpenTTD:
  - Microsoft Visual C++ (MSVC) 2003 and earlier.
  - GNU Compiler Collection (GCC) 3.2 and earlier.
    These old versions fail due to OpenTTD's template usage.
  - Intel C++ Compiler (ICC) 11.1 and earlier.
    Version 10.0 and earlier fail a configure check and fail with recent system
        headers.
    Version 10.1 fails to compile station_gui.cpp.
    Version 11.1 fails with internal error when compiling network.cpp.
  - Clang/LLVM 2.8 and earlier.
  - (Open) Watcom.

If any of these compilers can compile OpenTTD again, please let us know.
Patches to support more compilers are welcome.


8.0) Translating
---- -----------
See http://www.openttd.org/development for up-to-date information.

The use of the online Translator service, located at
http://translator.openttd.org/, is highly encouraged. For getting an account
simply follow the guidelines in the FAQ of the translator website.

If for some reason the website is down for a longer period of time, the
information below might be of help.

Please contact the translations manager (http://www.openttd.org/contact)
before beginning the translation process! This avoids double work, as
someone else may have already started translating to the same language.

8.1) Translation
---- -----------
So, now that you've notified the development team about your intention to
translate (You did, right? Of course you did.) you can pick up english.txt
(found in the SVN repository under /src/lang) and translate.

You must change the first two lines of the file appropriately:

##name English-Name-Of-Language
##ownname Native-Name-Of-Language

Note: Do not alter the following parts of the file:

    * String identifiers (the first word on each line)
    * Parts of the strings which are in curly braces (such as {STRING})
    * Lines beginning with ## (such as ##id), other than the first two lines of
      the file

8.2) Previewing
---- ----------
In order to view the translation in the game, you need to compile your language
file with the strgen utility. You can download the precompiled strgen from:
http://www.openttd.org/download-strgen
To compile it yourself just take the normal OpenTTD sources and build that.
During the build process the strgen utility will be made.

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

The most frequent problem is missing data files. Please install OpenGFX and
possibly OpenSFX and OpenMSX. See section 4.1.1 for more information.

Under certain circumstance, especially on Ubuntu OpenTTD can be extremely slow
and/or freeze. See known-bugs.txt for more information and how to solve this
problem on your computer.

Under Windows 98 and lower it is impossible to use a dedicated server; it will
fail to start. Perhaps this is for the better because those OSes are not known
for their stability.

With the added support for font-based text selecting a non-latin language can
result in lots of question marks ('?') being shown on screen. Please open your
configuration file (openttd.cfg - see Section 4.2 for where to find it)
and add a suitable font for the small, medium and / or large font, e.g.:
	small_font = "Tahoma"
	medium_font = "Tahoma"
	large_font = "Tahoma"
You should use a font name like "Tahoma" or a path to the desired font.

Any NewGRF file used in a game is stored inside the savegame and will refuse
to load if you don't have that NewGRF file available. A list of missing files
can be viewed in the NewGRF window accessible from the file load dialogue window.

You can try to obtain the missing files from that NewGRF dialogue or - if they
are not available online - you can search manually through our forum's graphics
development section (http://www.tt-forums.net/viewforum.php?f=66) or GrfCrawler
(http://grfcrawler.tt-forums.net/). Put the NewGRF files in OpenTTD's data folder
(see section 4.2 "OpenTTD directories") and rescan the list of available NewGRFs.
Once you have all missing files, you are set to go.


X.X) Credits
---- -------
The OpenTTD team (in alphabetical order):
  Albert Hofkamp (Alberth)        - GUI expert
  Jean-François Claeys (Belugas)  - GUI, newindustries and more
  Matthijs Kooijman (blathijs)    - Pathfinder-guru, pool rework
  Christoph Elsenhans (frosch)    - General coding
  Loïc Guilloux (glx)             - Windows Expert
  Michael Lutz (michi_cc)         - Path based signals
  Owen Rudge (orudge)             - Forum host, OS/2 port
  Peter Nelson (peter1138)        - Spiritual descendant from newGRF gods
  Ingo von Borstel (planetmaker)  - Support
  Remko Bijker (Rubidium)         - Lead coder and way more
  Zdeněk Sojka (SmatZ)            - Bug finder and fixer
  José Soler (Terkhen)            - General coding
  Thijs Marinussen (Yexo)         - AI Framework

Inactive Developers:
  Bjarni Corfitzen (Bjarni)       - MacOSX port, coder and vehicles
  Victor Fischer (Celestar)       - Programming everywhere you need him to
  Tamás Faragó (Darkvater)        - Ex-Lead coder
  Jaroslav Mazanec (KUDr)         - YAPG (Yet Another Pathfinder God) ;)
  Jonathan Coome (Maedhros)       - High priest of the NewGRF Temple
  Attila Bán (MiHaMiX)            - WebTranslator 1 and 2
  Christoph Mallon (Tron)         - Programmer, code correctness police

Retired Developers:
  Ludvig Strigeus (ludde)         - OpenTTD author, main coder (0.1 - 0.3.3)
  Serge Paquet (vurlix)           - Assistant project manager, coder (0.1 - 0.3.3)
  Dominik Scherer (dominik81)     - Lead programmer, GUI expert (0.3.0 - 0.3.6)
  Benedikt Brüggemeier (skidd13)  - Bug fixer and code reworker
  Patric Stout (TrueLight)        - Programmer (0.3 - pre0.7), sys op (active)

Thanks to:
  Josef Drexler                   - For his great work on TTDPatch.
  Marcin Grzegorczyk              - For his TTDPatch work and documentation of Transport Tycoon Deluxe internals and graphics (signals and track foundations)
  Petr Baudiš (pasky)             - Many patches, newgrf support, etc.
  Simon Sasburg (HackyKid)        - For the many bugfixes he has blessed us with
  Stefan Meißner (sign_de)        - For his work on the console
  Mike Ragsdale                   - OpenTTD installer
  Cian Duffy (MYOB)               - BeOS port / manual writing
  Christian Rosentreter (tokai)   - MorphOS / AmigaOS port
  Richard Kempton (RichK67)       - Additional airports, initial TGP implementation
  Alberto Demichelis              - Squirrel scripting language
  L. Peter Deutsch                - MD5 implementation
  Michael Blunck                  - For revolutionizing TTD with awesome graphics
  George                          - Canal graphics
  Andrew Parkhouse (andythenorth) - River graphics
  David Dallaston (Pikka)         - Tram tracks
  All Translators                 - For their support to make OpenTTD a truly international game
  Bug Reporters                   - Thanks for all bug reports
  Chris Sawyer                    - For an amazing game!
