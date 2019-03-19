# OpenTTD

## Table of contents

- 1.0) [About](#10-about)
- 2.0) [Contacting](#20-contacting)
    - 2.1) [Reporting bugs](#21-reporting-bugs)
    - 2.2) [Reporting desyncs](#22-reporting-desyncs)
- 3.0) [Supported platforms](#30-supported-platforms)
- 4.0) [Installing and running OpenTTD](#40-installing-and-running-openttd)
    - 4.1) [(Required) 3rd party files](#41-required-3rd-party-files)
    - 4.2) [OpenTTD directories](#42-openttd-directories)
    - 4.3) [Portable installations (portable media)](#43-portable-installations-portable-media)
    - 4.4) [Files in tar (archives)](#44-files-in-tar-archives)
- 5.0) [OpenTTD features](#50-openttd-features)
    - 5.1) [Logging of potentially dangerous actions](#51-logging-of-potentially-dangerous-actions)
    - 5.2) [Frame rate and performance metrics](#52-frame-rate-and-performance-metrics)
- 6.0) [Configuration file](#60-configuration-file)
- 7.0) [Compiling](#70-compiling)
    - 7.1) [Required/optional libraries](#71-requiredoptional-libraries)
    - 7.2) [Supported compilers](#72-supported-compilers)
    - 7.3) [Compilation of base sets](#73-compilation-of-base-sets)
- 8.0) [Translating](#80-translating)
    - 8.1) [Translation](#81-translation)
    - 8.2) [Previewing](#82-previewing)
- 9.0) [Troubleshooting](#90-troubleshooting)
- 10.0) [Licensing](#100-licensing)
- X.X) [Credits](#xx-credits)

## 1.0) About

OpenTTD is a transport simulation game based upon the popular game Transport
Tycoon Deluxe, written by Chris Sawyer. It attempts to mimic the original
game as closely as possible while extending it with new features.

OpenTTD is licensed under the GNU General Public License version 2.0,
but includes some 3rd party software under different licenses. See the
section "Licensing" below for details.

## 2.0) Contacting

The easiest way to contact the OpenTTD team is by submitting bug reports or
posting comments in our forums. You can also chat with us on IRC (#openttd
on irc.oftc.net).

The OpenTTD homepage is https://www.openttd.org.

You can also find the OpenTTD forums at
[https://www.tt-forums.net](https://www.tt-forums.net/viewforum.php?f=55).

### 2.1) Reporting bugs

First of all, check whether the bug is not already known. Do this by looking
through the file called 'known-bugs.txt' which is distributed with OpenTTD
like this readme.

For tracking our bugs we are using GitHub's issue tracker. You can find
the tracker at https://github.com/OpenTTD/OpenTTD/issues. Before actually
reporting take a look through the already reported bugs there to see if
the bug is already known. The 'known-bugs.txt' file might be a bit outdated
at the moment you are reading it as only bugs known before the release
are documented there. Also look through the recently closed bugs.

When you are sure it is not already reported you should:

- Make sure you are running a recent version, i.e. run the latest stable or
   nightly based on where you found the bug.
- Make sure you are not running a non-official binary, like a patch pack.
   When you are playing with a patch pack you should report any bugs to the
   forum thread related to that patch pack.
- Make it reproducible for the developers. In other words, create a savegame
   in which you can reproduce the issue once loaded. It is very useful to give
   us the crash.dmp, crash.sav, crash.log and crash screenshot which are
   created on crashes.
- Check whether the bug is already reported on our bug tracker. This includes
   searching for recently closed bug reports as the bug might already be fixed.

After you have done all that you can report the bug. Please include the
following information in your bug report:

- OpenTTD version (PLEASE test the latest Git revision/nightly build)
- Bug details, including instructions how to reproduce it
- Platform (Windows, Linux, FreeBSD, …) and compiler (including version) if
   you compiled OpenTTD yourself.
- The processor architecture of your OS (32-bit Windows, 64-bit Windows,
   Linux on an ARM, Mac OS X on a PowerPC, etc.)
- Attach a saved game **and** a screenshot if possible
- If this bug only occurred recently please note the last version without
   the bug and the first version including the bug. That way we can fix it
   quicker by looking at the changes made.
- Attach crash.dmp, crash.log and crash.sav. These files are usually created
   next to your openttd.cfg. The crash handler will tell you the location.

### 2.2) Reporting desyncs

As desyncs are hard to make reproducible OpenTTD has the ability to log all
actions done by clients so we can replay the whole game in an effort to make
desyncs better reproducible. You need to turn this ability on. When turned
on an automatic savegame will be made once the map has been constructed in
the 'save/autosave' directory, see OpenTTD directories to know where to find
this directory. Furthermore the log file 'commands-out.log' will be created
and all actions will be written to there.

To enable the desync debugging you need to set the debug level for 'desync'
to at least 1. You do this by starting OpenTTD with '`-d desync=<level>`' as
parameter or by typing '`debug_level desync=<level>`' in OpenTTD's internal
console.
The desync debug levels are:

- 0: nothing.
- 1: dumping of commands to 'commands-out.log'.
- 2: same as 1 plus checking vehicle caches and dumping that too.
- 3: same as 2 plus monthly saves in autosave.
- 4 and higher: same as 3

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
   whether we can quickly reproduce the desync. Otherwise we will need …
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

## 3.0) Supported platforms

OpenTTD has been ported to several platforms and operating systems. It should
not be very difficult to port it to a new platform. The currently working
platforms are:

- FreeBSD (SDL)
- Haiku (SDL)
- Linux (SDL or Allegro)
- macOS (universal) (Cocoa video and sound drivers)
- OpenBSD (SDL)
- OS/2 (SDL)
- Windows (Win32 GDI (faster) or SDL or Allegro)

## 4.0) Installing and running OpenTTD

Installing OpenTTD is fairly straightforward. Either you have downloaded an
archive which you have to extract to a directory where you want OpenTTD to
be installed, or you have downloaded an installer, which will automatically
extract OpenTTD in the given directory.

OpenTTD looks in multiple locations to find the required data files (described
in section 4.2). Installing any 3rd party files into a 'shared' location has
the advantage that you only need to do this step once, rather than copying the
data files into all OpenTTD versions you have.

Savegames, screenshots, etc are saved relative to the config file (openttd.cfg)
currently being used. This means that if you use a config file in one of the
shared directories, savegames will reside in the save/ directory next to the
openttd.cfg file there.

If you want savegames and screenshots in the directory where the OpenTTD binary
resides, simply have your config file in that location. But if you remove this
config file, savegames will still be in this directory (see notes in
section 4.2 'OpenTTD directories')

OpenTTD comes without AIs, so if you want to play with AIs you have to download
them. The easiest way is via the 'Check Online Content' button in the main menu.
You can select some AIs that you think are compatible with your playing style.
Another way is manually downloading the AIs from the forum although then you
need to make sure that you install all the required AI libraries too; they get
automatically selected (and downloaded) if you get the AIs via the 'Check
Online Content'. If you do not have an AI but have configured OpenTTD to start
an AI a message will be shown that the 'dummy' AI has been started.

### 4.1) (Required) 3rd party files

Before you run OpenTTD, you need to put the game's data files into a baseset/
directory which can be located in various places addressed in the following
section.

For OpenTTD you need to acquire some third party data files. For this you have
the choice of using the original Transport Tycoon Deluxe data files or a set
of free data files.

Do NOT copy files included with OpenTTD into 'shared' directories (explained in
the following sections) as sooner or later you will run into graphical glitches
when using other versions of the game.

#### 4.1.1) Free graphics and sound files

The free data files, split into OpenGFX for graphics, OpenSFX for sounds and
OpenMSX for music can be found at:

- https://www.openttd.org/download-opengfx for OpenGFX
- https://www.openttd.org/download-opensfx for OpenSFX
- https://www.openttd.org/download-openmsx for OpenMSX

Please follow the readme of these packages about the installation procedure.
The Windows installer can optionally download and install these packages.

#### 4.1.2) Original Transport Tycoon Deluxe graphics and sound files

If you want to play with the original Transport Tycoon Deluxe data files you
have to copy the data files from the CD-ROM into the baseset/ directory. It
does not matter whether you copy them from the DOS or Windows version of
Transport Tycoon Deluxe. The Windows install can optionally copy these files.
You need to copy the following files:
- sample.cat
- trg1r.grf or TRG1.GRF
- trgcr.grf or TRGC.GRF
- trghr.grf or TRGH.GRF
- trgir.grf or TRGI.GRF
- trgtr.grf or TRGT.GRF

#### 4.1.3) Original Transport Tycoon Deluxe music

If you want the Transport Tycoon Deluxe music, copy the appropriate files from
the original game into the baseset folder.
- TTD for Windows: All files in the gm/ folder (gm_tt00.gm up to gm_tt21.gm)
- TTD for DOS: The GM.CAT file
- Transport Tycoon Original: The GM.CAT file, but rename it to GM-TTO.CAT

#### 4.1.4) AIs

If you want AIs use the in-game content downloader. If for some reason that is
not possible or you want to use an AI that has not been uploaded to the content
download system download the tar file and place it in the ai/ directory. If the
AI needs libraries you will have to download those too and put them in the
ai/library/ directory. All AIs and AI Libraries that have been uploaded to
the content download system can be found at https://noai.openttd.org/downloads.
The AIs and libraries can be found their in the form of .tar.gz packages.
OpenTTD can read inside tar files but it does not extract .tar.gz files by itself.

To figure out which libraries you need for an AI you have to start the AI and
wait for an error message to pop up. The error message will tell you
'could not find library "lib-name"'. Download that library and try again.

#### 4.1.5) Game scripts

If you want an extra challenge in OpenTTD you can download so-called game
scripts via the in-game content downloader. These game scripts have varying
functionality, though they can generally influence town growth, subsidies, add
goals to reach or provide a different ranking system.

If you download a game script manually you have to follow the same rules as for
AIs, except that game scripts are placed in the game/ directory instead of the
ai/ directory.

### 4.2) OpenTTD directories

OpenTTD uses its own directory to store its required 3rd party base set files
(see section 4.1 'Required 3rd party files') and non-compulsory extension and
configuration files. See below for their proper place within this OpenTTD main
data directory.

The main OpenTTD directories can be found in various locations, depending on
your operating system:

1. The current working directory (from where you started OpenTTD)

    For non-Windows operating systems OpenTTD will not scan for files in this
    directory if it is your personal directory, i.e. '~/', or when it is the
    root directory, i.e. '/'.

2. Your personal directory
    - Windows:
        - `C:\My Documents\OpenTTD` (95, 98, ME)
        - `C:\Documents and Settings\<username>\My Documents\OpenTTD` (2000, XP)
        - `C:\Users\<username>\Documents\OpenTTD` (Vista, 7, 8.1, 10)
    - macOS: `~/Documents/OpenTTD`
    - Linux: `$XDG_DATA_HOME/openttd` which is usually `~/.local/share/openttd`
       when built with XDG base directory support, otherwise `~/.openttd`
3. The shared directory
    - Windows:
        - `C:\Documents and Settings\All Users\Shared Documents\OpenTTD` (2000, XP)
        - `C:\Users\Public\Documents\OpenTTD` (Vista, 7, 8.1, 10)
    - macOS: `/Library/Application Support/OpenTTD`
    - Linux: not available
4. The binary directory (where the OpenTTD executable is)
    - Windows: `C:\Program Files\OpenTTD`
    - Linux: `/usr/games`
5. The installation directory (Linux only)
    - Linux: `/usr/share/games/openttd`
6. The application bundle (macOS only)

    It includes the OpenTTD files (grf+lng) and it will work as long as they
    are not touched

Different types of data or extensions go into different subdirectories of the
chosen main OpenTTD directory:

| data type           | directory         | additional info             |
| ------------------- | ----------------- | --------------------------- |
| Config File         | (no subdirectory) |                             |
| Screenshots         | screenshot        |                             |
| Base Graphics       | baseset           | (or a subdirectory thereof) |
| Sound Sets          | baseset           | (or a subdirectory thereof) |
| NewGRFs             | newgrf            | (or a subdirectory thereof) |
| 32bpp Sets          | newgrf            | (or a subdirectory thereof) |
| Music Sets          | baseset           | (or a subdirectory thereof) |
| AIs                 | ai                | (or a subdirectory thereof) |
| AI Libraries        | ai/library        | (or a subdirectory thereof) |
| Game Scripts (GS)   | game              | (or a subdirectory thereof) |
| GS Libraries        | game/library      | (or a subdirectory thereof) |
| Savegames           | save              |                             |
| Automatic Savegames | save/autosave     |                             |
| Scenarios           | scenario          |                             |

The (automatically created) directory content_download is for OpenTTD's internal
use and no files should be added to it or its subdirectories manually.

#### Notes:

- Linux in the previous list means .deb, but most paths should be similar for
   others.
- The previous search order is also used for NewGRFs and openttd.cfg.
- If openttd.cfg is not found, then it will be created using the 2, 4, 1, 3,
   5 order. When built with XDG base directory support, openttd.cfg will be
   created in $XDG_CONFIG_HOME/openttd which is usually ~/.config/openttd.
- Savegames will be relative to the config file only if there is no save/
   directory in paths with higher priority than the config file path, but
   autosaves and screenshots will always be relative to the config file.
   Unless the configuration file is in $XDG_CONFIG_HOME/openttd, then all
   other files will be saved under $XDG_DATA_HOME/openttd.

#### The preferred setup:

Place 3rd party files in shared directory (or in personal directory if you do
not have write access on shared directory) and have your openttd.cfg config
file in personal directory (where the game will then also place savegames and
screenshots).

### 4.3) Portable installations (portable media)

You can install OpenTTD on external media so you can take it with you, i.e.
using a USB key, or a USB HDD, etc.
Create a directory where you shall store the game in (i.e. OpenTTD/).
Copy the binary (OpenTTD.exe, OpenTTD.app, openttd, etc), baseset/ and your
openttd.cfg to this directory.
You can copy binaries for any operating system into this directory, which will
allow you to play the game on nearly any computer you can attach the external
media to.
As always - additional grf files are stored in the newgrf/ dir (for details,
again, see section 4.1).

### 4.4) Files in tar (archives)

OpenTTD can read files that are in an uncompressed tar (archive), which
makes it easy to bundle files belonging to the same script, NewGRF or base
set. Music sets are the only exception as they cannot be stored in a tar
file due to being played by external applications.

OpenTTD sees each tar archive as the 'root' of its search path. This means that
having a file with the same path in two different tar files means that one
cannot be opened, after all only one file will be found first. As such it is
advisable to put an uniquely named folder in the root of the tar and put all the
content in that folder. For example, all downloaded content has a path that
concatenates the name of the content and the version, which makes the path
unique. For custom tar files it is advised to do this as well.

The normal files are also referred to by their relative path from the search
directory, this means that also normal files could hide files in a tar as
long as the relative path from the search path of the normal file is the
same as the path in the tar file. Again it is advised to have an unique path
to the normal file so they do not collide with the files from other tar
files.

## 5.0) OpenTTD features

OpenTTD has a lot of features going beyond the original Transport Tycoon Deluxe
emulation. Unfortunately, there is currently no comprehensive list of features,
but there is a basic features list on the web, and some optional features can be
controlled through the Advanced Settings dialog. We also implement some
features known from [TTDPatch](https://www.ttdpatch.net).

Several important non-standard controls:

- Ctrl modifies many commands and makes them more powerful. For example Ctrl
   clicking on signals with the build signal tool changes their behaviour,
   holding Ctrl while the track build tool is activated changes it to the track
   removal tool, and so on. See https://wiki.openttd.org/Hidden_features
   for a non-comprehensive list or look at the tooltips.
- Ingame console. More information at https://wiki.openttd.org/Console
- Hovering over a GUI element shows tooltips. This can be changed to right click
   via the advanced settings.

### 5.1) Logging of potentially dangerous actions

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

- Adding / removing / changing order of NewGRFs
- Changing NewGRF parameters, loading compatible NewGRF
- Changing game mode (scenario editor <-> normal game)
- Loading game saved in a different OpenTTD / TTDPatch / Transport Tycoon Deluxe /
   original Transport Tycoon version
- Running a modified OpenTTD build
- Changing settings affecting NewGRF behaviour (non-network-safe settings)
- Triggering NewGRF bugs

No personal information is stored.

You can show the game log by typing 'gamelog' in the console or by running
OpenTTD in debug mode.

### 5.2) Frame rate and performance metrics

The Help menu in-game has a function to open the Frame rate window. This
window shows various real-time performance statistics, measuring what parts
of the game require the most processing power currently.

A summary of the statistics can also be retrieved from the console with the
`fps` command. This is especially useful on dedicated servers, where the
administrator might want to determine what's limiting performance in a slow
game.

The frame rate is given as two figures, the simulation rate and the graphics
frame rate. Usually these are identical, as the screen is rendered exactly
once per simulated tick, but in the future there might be support for graphics
and simulation running at different rates. When the game is paused, the
simulation rate drops to zero.

In addition to the simulation rate, a game speed factor is also calculated.
This is based on the target simulation speed, which is 30 milliseconds per
game tick. At that speed, the expected frame rate is 33.33 frames/second, and
the game speed factor is how close to that target the actual rate is. When
the game is in fast forward mode, the game speed factor shows how much
speed up is achieved.

The lower part of the window shows timing statistics for individual parts of
the game. The times shown are short-term and long-term averages of how long
it takes to process one tick of game time, all figures are in milliseconds.

Clicking a line in the lower part of the window opens a graph window, giving
detailed readings on each tick simulated by the game.

The following is an explanation of the different statistics:

- *Game loop* - Total processing time used per simulated "tick" in the game.
  This includes all pathfinding, world updates, and economy handling.
- *Cargo handling* - Time spent loading/unloading cargo at stations, and
  industries and towns sending/retrieving cargo from stations.
- *Train ticks*, *Road vehicle ticks*, *Ship ticks*, *Aircraft ticks* -
  Time spent on pathfinding and other processing for each player vehicle type.
- *World ticks* - Time spent on other world/landscape processing. This
  includes towns growing, building animations, updates of farmland and trees,
  and station rating updates.
- *GS/AI total*, *Game script*, and *AI players* - Time spent running logic
  for game scripts and AI players. The total may show as less than the current
  sum of the individual scripts, this is because AI players at lower
  difficulty settings do not run every game tick, and hence contribute less
  to the average across all ticks. Keep in mind that the "Current" figure is
  also an average, just only over short term.
- *Link graph delay* - Time overruns of the cargo distribution link graph
  update thread. Usually the link graph is updated in a background thread,
  but these updates need to synchronise with the main game loop occasionally,
  if the time spent on link graph updates is longer than the time taken to
  otherwise simulate the game while it was updating, these delays are counted
  in this figure.
- *Graphics rendering* - Total time spent rendering all graphics, including
  both GUI and world viewports. This typically spikes when panning the view
  around, and when more things are happening on screen at once.
- *World viewport rendering* - Isolated time spent rendering just world
  viewports. If this figure is significantly lower than the total graphics
  rendering time, most time is spent rendering GUI than rendering world.
- *Video output* - Speed of copying the rendered graphics to the display
  adapter. Usually this should be very fast (in the range of 0-3 ms), large
  values for this can indicate a graphics driver problem.
- *Sound mixing* - Speed of mixing active audio samples together. Usually
  this should be very fast (in the range of 0-3 ms), if it is slow, consider
  switching to the NoSound set.

If the frame rate window is shaded, the title bar will instead show just the
current simulation rate and the game speed factor.

## 6.0) Configuration file

The configuration file for OpenTTD (openttd.cfg) is in a simple Windows-like
.INI format. It is mostly undocumented. Almost all settings can be changed
ingame by using the 'Advanced Settings' window.
When you cannot find openttd.cfg you should look in the directories as
described in section 4.2. If you do not have an openttd.cfg OpenTTD will
create one after closing.

## 7.0) Compiling

### Windows:

You need Microsoft Visual Studio 2015 Update 3 or more recent. Open the project file
and it should build automatically. In case you want to build with SDL support
you need to add WITH_SDL to the project settings.

PNG (WITH_PNG), ZLIB (WITH_ZLIB), LZO (WITH_LZO), Freetype (WITH_FREETYPE) and
libLZMA (WITH_LIBLZMA) support is enabled by default. For these to work you need their
development files. To get them just use vcpkg from https://github.com/Microsoft/vcpkg
using x86-windows-static and x64-windows-static triplets.
For more help with VS see docs/Readme_Windows_MSVC.md.

You can also build it using the Makefile with MSYS/MinGW or Cygwin/MinGW.
Please read the Makefile for more information.

### Solaris, FreeBSD, OpenBSD:

Use '`gmake`', but do a '`./configure`' before the first build.

### Linux/Unix:

OpenTTD can be built with GNU '`make`'. On non-GNU systems it is called '`gmake`'.
However, for the first build one has to do a '`./configure`' first.

### macOS:

Use '`make`' or Xcode (which will then call make for you)
This will give you a binary for your CPU type (PPC/Intel)
However, for the first build one has to do a '`./configure`' first.
To make a universal binary type '`./configure --enabled-universal`'
instead of '`./configure`'.

### Haiku:

Use '`make`', but do a '`./configure`' before the first build.

### OS/2:

A comprehensive GNU build environment is required to build the OS/2 version.
See the docs/Readme_OS2.txt file for more information.

### 7.1) Required/optional libraries

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
Without libSDL/liballegro on non-Windows and non-macOS machines you have
no graphical user interface; you would be building a dedicated server.

### 7.2) Supported compilers

The following compilers are known to compile OpenTTD:

- Microsoft Visual C++ (MSVC) 2015, 2017 and 2019.
- GNU Compiler Collection (GCC) 3.3 - 4.4, 4.6 - 4.8.
    - Versions 4.1 and earlier give bogus warnings about uninitialised variables.
    - Versions 4.4, 4.6 give bogus warnings about freeing non-heap objects.
    - Versions 4.6 and later give invalid warnings when lto is enabled.
- Intel C++ Compiler (ICC) 12.0.
- Clang/LLVM 2.9 - 3.0
   Version 2.9 gives bogus warnings about code nonconformity.

The following compilers are known not to compile OpenTTD:

- Microsoft Visual C++ (MSVC) 2013 and earlier.
   These old versions do not support modern C++ language features.
- GNU Compiler Collection (GCC) 3.2 and earlier.
   These old versions fail due to OpenTTD's template usage.
- GNU Compiler Collection (GCC) 4.5. It optimizes enums too aggressively.
   See https://github.com/OpenTTD/OpenTTD/issues/5513 and references therein.
- Intel C++ Compiler (ICC) 11.1 and earlier.
    - Version 10.0 and earlier fail a configure check and fail with recent
       system headers.
    - Version 10.1 fails to compile station_gui.cpp.
    - Version 11.1 fails with an internal error when compiling network.cpp.
- Clang/LLVM 2.8 and earlier.
- (Open) Watcom.

If any of these compilers can compile OpenTTD again, please let us know.
Patches to support more compilers are welcome.

### 7.3) Compilation of base sets

To recompile the extra graphics needed to play with the original Transport
Tycoon Deluxe graphics you need GRFCodec (which includes NFORenum) as well.
GRFCodec can be found at https://www.openttd.org/download-grfcodec.
The compilation of these extra graphics does generally not happen, unless
you remove the graphics file using '`make maintainer-clean`'.

Re-compilation of the base sets, thus also use of '`--maintainer-clean`' can
leave the repository in a modified state as different grfcodec versions can
cause binary differences in the resulting grf. Also translations might have
been added for the base sets which are not yet included in the base set
information files. Use the configure option '`--without-grfcodec`' to avoid
modification of the base set files by the build process.

## 8.0) Translating

See https://www.openttd.org/development for up-to-date information.

The use of the online Translator service, located at
https://translator.openttd.org, is highly encouraged. For getting an account
simply follow the guidelines in the FAQ of the translator website.

If for some reason the website is down for a longer period of time, the
information below might be of help.

Please contact the translations manager (https://www.openttd.org/contact)
before beginning the translation process! This avoids double work, as
someone else may have already started translating to the same language.

### 8.1) Translation

So, now that you have notified the development team about your intention to
translate (You did, right? Of course you did.) you can pick up english.txt
(found in the Git repository under /src/lang) and translate.

You must change the first two lines of the file appropriately:

    ##name English-Name-Of-Language
    ##ownname Native-Name-Of-Language

Note: Do not alter the following parts of the file:

- String identifiers (the first word on each line)
- Parts of the strings which are in curly braces (such as {STRING})
- Lines beginning with ## (such as ##id), other than the first two lines
   of the file

### 8.2) Previewing

In order to view the translation in the game, you need to compile your language
file with the strgen utility. As this utility is tailored to a specific OpenTTD
version, you need to compile it yourself. Just take the normal OpenTTD sources
and build that. During the build process the strgen utility will be made.

strgen is a command-line utility. It takes the language filename as parameter.

Example:

    strgen lang/german.txt

This results in compiling german.txt and produces another file named german.lng.
Any missing strings are replaced with the English strings. Note that it looks
for english.txt in the lang subdirectory, which is where your language file
should also be.

That is all! You should now be able to select the language in the game options.

## 9.0) Troubleshooting

To see all startup options available to you, start OpenTTD with the
'`./openttd -h`' option. This might help you tweak some of the settings.

If the game is acting strange and you feel adventurous you can try the
'`-d [[<name>=]<level>]`' flag, where the higher levels will give you more
debugging output. The 'name' variable can help you to display only some type of
debugging messages. This is mostly undocumented so best is to look in the
source code file debug.c for the various debugging types. For more information
look at https://wiki.openttd.org/Command_line.

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

You should use a font name like 'Tahoma' or a path to the desired font.

Any NewGRF file used in a game is stored inside the savegame and will refuse to
load if you do not have that NewGRF file available. A list of missing files can
be viewed in the NewGRF window accessible from the file load dialogue window.

You can try to obtain the missing files from that NewGRF dialogue or – if they
are not available online – you can search manually through our
[forum's graphics development section](https://www.tt-forums.net/viewforum.php?f=66)
or [GRFCrawler](https://grfcrawler.tt-forums.net). Put the NewGRF files in
OpenTTD's newgrf folder (see section 4.2 'OpenTTD directories') and rescan the
list of available NewGRFs. Once you have all missing files, you are set to go.

## 10.0) Licensing

OpenTTD is licensed under the GNU General Public License version 2.0. For
the complete license text, see the file 'COPYING'. This license applies
to all files in this distribution, except as noted below.

The squirrel implementation in src/3rdparty/squirrel is licensed under
the Zlib license. See src/3rdparty/squirrel/COPYRIGHT for the complete
license text.

The md5 implementation in src/3rdparty/md5 is licensed under the Zlib
license. See the comments in the source files in src/3rdparty/md5 for
the complete license text.

The implementations of Posix getaddrinfo and getnameinfo for OS/2 in
src/3rdparty/os2 are distributed partly under the GNU Lesser General Public
License 2.1, and partly under the (3-clause) BSD license. The exact licensing
terms can be found in src/3rdparty/os2/getaddrinfo.c resp.
src/3rdparty/os2/getnameinfo.c.

CONTRIBUTING.md is adapted from
[Bootstrap](https://github.com/twbs/bootstrap/blob/master/CONTRIBUTING.md)
under the [Creative Commons Attribution 3.0 Unported
License](https://github.com/twbs/bootstrap/blob/master/docs/LICENSE)
terms for Bootstrap documentation.

## X.X) Credits

### The OpenTTD team (in alphabetical order):

- Grzegorz Duczyński (adf88) - General coding (since 1.7.2)
- Albert Hofkamp (Alberth) - GUI expert (since 0.7)
- Matthijs Kooijman (blathijs) - Pathfinder-guru, Debian port (since 0.3)
- Ulf Hermann (fonsinchen) - Cargo Distribution (since 1.3)
- Christoph Elsenhans (frosch) - General coding (since 0.6)
- Loïc Guilloux (glx) - Windows Expert (since 0.4.5)
- Michael Lutz (michi_cc) - Path based signals (since 0.7)
- Niels Martin Hansen (nielsm) - Music system, general coding (since 1.9)
- Owen Rudge (orudge) - Forum host, OS/2 port (since 0.1)
- Peter Nelson (peter1138) - Spiritual descendant from newGRF gods (since 0.4.5)
- Ingo von Borstel (planetmaker) - General coding, Support (since 1.1)
- Remko Bijker (Rubidium) - Lead coder and way more (since 0.4.5)
- José Soler (Terkhen) - General coding (since 1.0)
- Leif Linse (Zuu) - AI/Game Script (since 1.2)

### Inactive Developers:

- Jean-François Claeys (Belugas) - GUI, newindustries and more (0.4.5 - 1.0)
- Bjarni Corfitzen (Bjarni) - macOS port, coder and vehicles (0.3 - 0.7)
- Victor Fischer (Celestar) - Programming everywhere you need him to (0.3 - 0.6)
- Jaroslav Mazanec (KUDr) - YAPG (Yet Another Pathfinder God) ;) (0.4.5 - 0.6)
- Jonathan Coome (Maedhros) - High priest of the NewGRF Temple (0.5 - 0.6)
- Attila Bán (MiHaMiX) - WebTranslator 1 and 2 (0.3 - 0.5)
- Zdeněk Sojka (SmatZ) - Bug finder and fixer (0.6 - 1.3)
- Christoph Mallon (Tron) - Programmer, code correctness police (0.3 - 0.5)
- Patric Stout (TrueBrain) - NoProgrammer (0.3 - 1.2), sys op (active)
- Thijs Marinussen (Yexo) - AI Framework, General (0.6 - 1.3)

### Retired Developers:

- Tamás Faragó (Darkvater) - Ex-Lead coder (0.3 - 0.5)
- Dominik Scherer (dominik81) - Lead programmer, GUI expert (0.3 - 0.3)
- Emil Djupfeld (egladil) - macOS port (0.4 - 0.6)
- Simon Sasburg (HackyKid) - Bug fixer (0.4 - 0.4.5)
- Ludvig Strigeus (ludde) - Original author of OpenTTD, main coder (0.1 - 0.3)
- Cian Duffy (MYOB) - BeOS port / manual writing (0.1 - 0.3)
- Petr Baudiš (pasky) - Many patches, newgrf support, etc. (0.3 - 0.3)
- Benedikt Brüggemeier (skidd13) - Bug fixer and code reworker (0.6 - 0.7)
- Serge Paquet (vurlix) - 2nd contributor after ludde (0.1 - 0.3)

### Thanks to:

- Josef Drexler - For his great work on TTDPatch.
- Marcin Grzegorczyk - For his TTDPatch work and documentation of Transport Tycoon Deluxe internals and track foundations
- Stefan Meißner (sign_de) - For his work on the console
- Mike Ragsdale - OpenTTD installer
- Christian Rosentreter (tokai) - MorphOS / AmigaOS port
- Richard Kempton (RichK67) - Additional airports, initial TGP implementation
- Alberto Demichelis - Squirrel scripting language
- L. Peter Deutsch - MD5 implementation
- Michael Blunck - For revolutionizing TTD with awesome graphics
- George - Canal graphics
- Andrew Parkhouse (andythenorth) - River graphics
- David Dallaston (Pikka) - Tram tracks
- All Translators - For their support to make OpenTTD a truly international game
- Bug Reporters - Thanks for all bug reports
- Chris Sawyer - For an amazing game!
