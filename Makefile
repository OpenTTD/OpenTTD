# $Id$

##############################################################################
#
# Usage
#

# Synopsis:
#
# make WITH_ZLIB=1 UNIX=1 MANUAL_CONFIG=1
#
# (See below for the list of possible options.)
#
# Alternately, you can run make without the MANUAL_CONFIG part. It then
# generates Makefile.config, where you can customize all the options.
# However beware that for all subsequent calls the option values from
# Makefile.config take precedence to the commandline options.
#
# (That means that you probably want to either specify the options on command
# line together with MANUAL_CONFIG=1 or you want to specify no commandline
# options at all.)

# Targets:
#
# Defaults to building binary
# clean: remove intermediate build files
# mrproper: remove intermediate files and makefile configuration
# upgradeconf: add new options to old Makefile.config
# osx: OS X application
# release: used by OSX to make a dmg file ready to release

# Options:
#
# Summary of OS choice defines
# WIN32: building on Windows
# UNIX: building on *nix derivate (Linux, FreeBSD)
# OSX: building on Mac OS X
# MORPHOS: building on MorphOS
# BEOS: building on BeOS
# SUNOS: building on SunOS (Solaris)
#
# Summary of library choice defines
# WITH_ZLIB: savegames using zlib
# WITH_PNG: screenshots using PNG
# WITH_SDL: SDL video driver support
# WITH_COCOA: Cocoa video driver support
#
# Summary of other defines:
# DEBUG: build in debug mode
# PROFILE: build in profile mode, disables -s and -fomit-frame-pointer
# TRANSLATOR: build in translator mode (untranslated strings are prepended by
#             a <TODO> mark)
# RELEASE: this will be the released version number. It replaces all places
#          where it normally would print the revision number
# MIDI: if set, it will use it as custom path to midi player.
#       If unset, it will use the hardcoded path in the c code
#       This can still be overriden by the music.extmidi openttd.cfg option.
# WITH_DIRECTMUSIC: enable DirectMusic MIDI support
# WITH_NETWORK: enable networking
# DEDICATED: allows compilation on UNIX without SDL. Useful for dedicated servers
#
# Paths:
# INSTALL: If not set, the game uses the directory of the binary to
# store everything (lang, data, gm, save and openttd.cfg), this is the `old' behaviour.
# In this case, none of the following paths are used, you also should _not_
# use `make install', but copy the required stuff yourself (or just play out
# of you source directory, which should work fine).
# If you want to use `make install' to install the game globally, you should
# define it _before_ you build the game. If you only define INSTALL when you
# do `make install', the game won't be able to find it's files (so you should
# also define all the following paths before building).
#
# So, the following paths should be defined if INSTALL is defined.
# None of these paths have to end with /
# PREFIX:	Normally /usr/local
# BINARY_DIR:	The location of the binary, normally games. (Will be prefixed
#		with $PREFIX)
# DATA_DIR: 	The location of the data (lang, data, gm and scenario), normally
#		share/games/openttd. (Will be prefixed with $PREFIX) Note that scenarios
#		are only put here if USE_HOMEDIR is true, otherwise they are placed in
#		PERSONAL_DIR/scenario
# ICON_DIR:   The location of the openttd icon. (Will be prefixed with
# 	$PREFIX).
# PERSONAL_DIR:	The directory where openttd.cfg and the save folder will be
#		stored. You cannot use ~ here, define USE_HOMEDIR for that.
# USE_HOMEDIR:	If this variable is set, PERSONAL_DIR will be prefixed with
#		~/ at runtime (the user's homedir)
# SECOND_DATA_PATH  Use this data dir if a file is not found in the data dir in the data path
# CUSTOM_LANG_PATH  If this is set, it will use the path given to search for lng files
#		instead of the lang dir in the data path
#   NOTE: both SECOND_DATA_PATH and CUSTOM_LANG_PATH uses paths relative to where OTTD is opened
#
# DEST_DIR:	make install will use this directory instead of the filesystem
# 		root to install its files. This should normally not be used by
# 		ordinary users, currently it is only used for the debian
# 		packaging. This value should only be set when calling `make
# 		install' and is not saved in Makefile.config
#               (Note that DESTDIR is checked if DEST_DIR is not set.)
#
# STATIC: link statically
# CYGWIN: build in Cygwin environment
# MINGW: build with MingW compiler, link with MingW libraries
#
# VERBOSE: show full compiler invocations instead of brief progress messages
#
# Special for crosscompiling there are some commands available:
#
# UNIVERSAL_BINARY: builds a universal binary for OSX. Make sure you got both PPC and x86 libs. Only works with GCC 4 or newer
# TRIPLE_BINARY: builds a universal binary with the addition of code optimised for G5 (which means a total of 3 binaries in one file)
# OTTD_PPC, OTTD_PPC970, OTTD_i386: compile for target architecture.
#    Multiple flags can be used so OTTD_PPC:=1 OTTD_i386:=1 produces the same result as UNIVERSAL_BINARY
#
# JAGUAR: Crosscompiling for OSX 1.2.8 (codenamed Jaguar). Only works if OSX is defined too. Only works with GCC 4 or newer
#	This can be changed to any PPC version of OSX by changing the ppc flags in Makefile.config
#
# ENDIAN_FORCE: forces the endian-check to give a certain result. Can be BE, LE or PREPROCESSOR.
#	PREPROCESSOR is always used on all OSX targets and will make the preprocessor pick the right endian.
#	this means that you don't have to think about endianess when compiling for OSX.
#	Very useful for universal binaries and crosscompilers. Not sure if it works on non OSX targets
# WINDRES: the location of your windres
# CC_HOST: the gcc of your localhost if you are making a target that produces incompatible executables
# CFLAGS_HOST: cflags used for CC_HOST. Make it something if you are getting errors when you try to compi
#		windows executables on linux. (just: CFLAGS_HOST:='-I' or something)


##############################################################################
#
# Configuration
#


# Makefile version tag
# it checks if the version tag in Makefile.config is the same and force update outdated config files
MAKEFILE_VERSION:=10

# Automatic configuration
MAKE_CONFIG:=Makefile.config
MAKEFILE:=Makefile
LIB_DETECTION=makefiledir/Makefile.libdetection
CONFIG_WRITER=makefiledir/Makefile.config_writer

# Apply automatic configuration
# See target section for how this is built, suppress errors
# since first time it isn't found but make reads this twice
-include $(MAKE_CONFIG)


# updates Makefile.config if it's outdated
ifneq ($(MAKEFILE_VERSION),$(CONFIG_VERSION))
	UPDATECONFIG:=upgradeconf
	CONFIG_INCLUDED:=
endif

# this is used if there aren't any Makefile.config
ifndef CONFIG_INCLUDED
# sets network on by default if there aren't any config file
ENABLE_NETWORK:=1

# paths for make install
# disabled as they would break it for some (many?) people if they were default
#PREFIX:=/usr/local
#DATA_DIR:=share/games/openttd
#BINARY_DIR:=games
#PERSONAL_DIR:=.openttd
#USE_HOMEDIR:=1

-include $(LIB_DETECTION)
endif

ifdef SUPRESS_LANG_ERRORS
LANG_ERRORS =  >/dev/null 2>&1
endif

ifdef OSX
-include os/macosx/Makefile.setup
endif

ifdef STATIC
ifndef WIN32
ifndef OSX
ifndef MORPHOS
ifndef SKIP_STATIC_CHECK
$(error Static is only known to work on MorphOS and MacOSX!!! --- Check Makefile.config for more info and howto bypass this check)
endif
endif
endif
endif
endif

ifdef WITH_COCOA
ifdef WITH_SDL
$(error You can not use both the SDL video driver and the Cocoa video driver at the same time)
endif
ifdef DEDICATED
$(error You can not use the Cocoa video driver in a dedicated server)
endif
else
# Force SDL on UNIX platforms
ifndef WITH_SDL
ifdef UNIX
ifndef DEDICATED
$(error You need to have SDL installed in order to run OpenTTD on UNIX. Use DEDICATED if you want to compile a CLI based server)
endif
endif
endif
endif

# remove the dependancy for sdl if DEDICALTED is used
ifdef DEDICATED
	WITH_SDL:=
endif

# add -lpthread to LDFLAGS
ifndef WIN32
	ifndef MORPHOS
		ifndef OSX
			LDFLAGS+=-lpthread
		endif
	endif
endif

ifdef OSX
	LDFLAGS+=-framework Cocoa
endif

ifdef WITH_SDL
	ifndef SDL_CONFIG
$(error WITH_SDL can't be used when SDL_CONFIG is not set. Edit Makefile.config to correct this)
	endif
endif

ifdef WITH_PNG
	ifndef LIBPNG_CONFIG
$(error WITH_PNG can't be used when LIBPNG_CONFIG is not set. Edit Makefile.config to correct this)
	endif
endif

##############################################################################
#
# Compiler configuration
#

# Executable file extension
ifdef WIN32
EXE=.exe
else
EXE=
endif

# Set output executable names
TTD=openttd$(EXE)
ENDIAN_CHECK=endian_check$(EXE)
STRGEN=strgen/strgen$(EXE)
OSXAPP="OpenTTD.app"

ifdef RELEASE
REV:=$(RELEASE)
else
ifeq ($(shell if test -d .svn; then echo 1; fi), 1)
REV_MODIFIED := $(shell svnversion . | sed -n 's/.*\(M\).*/\1/p' )
REV := $(shell LC_ALL=C svn info | awk '/^URL:.*branch/ { BRANCH="-"a[split($$2, a, "/")] } /^Last Changed Rev:/ { REV="r"$$4"$(REV_MODIFIED)" } END { print REV BRANCH }')
endif
endif

# define flag to use for -lrt (some OSes overwrites this later for compatibility)
ifndef LRT
ifndef MORPHOS
LRT:= -lrt
endif
endif

# MorphOS needs builddate
BUILDDATE=`date +%d.%m.%y`

# Check if there is a windres override
ifndef WINDRES
WINDRES = windres
endif

# Check that CXX is defined. If not, then it's g++
ifndef CXX
CXX = g++
endif

# Check if CXX_HOST is defined. If not, it is CXX
ifndef CXX_HOST
CXX_HOST = $(CXX)
endif

# Check if we have a new target
ifndef CXX_TARGET
CXX_TARGET = $(CXX_HOST)
endif

# Check if CC_HOST is defined. If not, it is CC
ifndef CC_HOST
CC_HOST = $(CC)
endif

ifndef CFLAGS_HOST
CFLAGS_HOST = $(BASECFLAGS)
endif

# Check if we have a new target
ifndef CC_TARGET
CC_TARGET = $(CC_HOST)
endif

CC_VERSION = $(shell $(CC_TARGET) -dumpversion | cut -c 1,3)

# GNU make can only test for (in)equality
# this is a workaround to test for >=
ifeq ($(shell expr $(CC_VERSION) \>= 29), 1)
  CFLAGS += -O -Wall -Wno-multichar -Wsign-compare -Wundef
  CC_CFLAGS += -Wstrict-prototypes
  CFLAGS += -Wwrite-strings -Wpointer-arith
endif
ifeq ($(shell expr $(CC_VERSION) \>= 30), 1)
  CFLAGS += -W -Wno-unused-parameter
endif
ifeq ($(shell expr $(CC_VERSION) \>= 34), 1)
  CC_CFLAGS += -Wdeclaration-after-statement -Wold-style-definition
endif

ifdef DEBUG
  ifeq ($(shell expr $(DEBUG) \>= 1), 1)
    CFLAGS += -g -D_DEBUG
  endif
  ifeq ($(shell expr $(DEBUG) \>= 2), 1)
    CFLAGS += -fno-inline
  endif
  ifeq ($(shell expr $(DEBUG) \>= 3), 1)
    CFLAGS += -O0
  endif
endif

ifdef PROFILE
  CFLAGS += -pg
  LDFLAGS += -pg
  ifdef OSX
  # Shark (Xcode's profiling tool) needs -g to relate CPU usage to line numbers in the source code
    BASECFLAGS += -g
  endif
endif

CDEFS=-DWITH_REV

ifndef DEBUG
ifndef PROFILE
# Release mode
ifndef MORPHOS
ifndef IRIX
# automatical strip breaks under morphos
ifdef OSX
# it appears that OSX can't handle automated stripping when mixing C and C++
# we will do it manually in the target OSX_STRIP
OSX_STRIP:=OSX_STRIP
else
LDFLAGS += -s
endif
endif
endif
endif

ifdef OSX
# these compilerflags makes the app run as fast as possible without making the app unstable. It works on G3 or newer
BASECFLAGS += -O3 -funroll-loops -fsched-interblock -falign-loops=16 -falign-jumps=16 -falign-functions=16 -falign-jumps-max-skip=15 -falign-loops-max-skip=15 -mdynamic-no-pic
else
ifdef MORPHOS
BASECFLAGS += -I/gg/os-include -O2 -noixemul -fstrict-aliasing -fexpensive-optimizations
BASECFLAGS += -mcpu=604 -fno-inline -mstring -mmultiple
else
BASECFLAGS += -O2
endif
ifndef PROFILE
ifndef IRIX
BASECFLAGS += -fomit-frame-pointer
endif
endif
endif
endif

ifdef STATIC
ifndef OSX	# OSX can't build static if -static flag is used
LDFLAGS += -static
endif
endif

# If building on MingW don't link with Cygwin libs
ifdef WIN32
ifdef CYGWIN
BASECFLAGS += -mwin32
LDFLAGS += -mwin32
endif
ifdef MINGW
BASECFLAGS += -mno-cygwin
LDFLAGS += -mno-cygwin
# -lrt fails with MINGW, so we disable it
LRT:=
endif
endif

CFLAGS += $(BASECFLAGS)

ifdef UNIX
CDEFS += -DUNIX
endif

ifdef BEOS
CDEFS += -DBEOS
LDFLAGS += -lmidi -lbe -lpthread
ifdef WITH_NETWORK
	ifdef BEOS_NET_SERVER
		CDEFS += -DBEOS_NET_SERVER
		LDFLAGS += -lnet
	else
		# BONE needs a few more libraries than R5
		LDFLAGS += -lbind -lsocket
	endif
endif
endif

ifdef MORPHOS
# -Wstrict-prototypes generates much noise because of system headers
# and it also uses 4-byte bools in the C++ ABI, so C bools need to be that size as well for YAPF to work
CFLAGS += -Wno-strict-prototypes -DFOUR_BYTE_BOOL
endif

ifdef SUNOS
CDEFS += -DSUNOS
ifdef WITH_NETWORK
LDFLAGS += -lnsl -lsocket
endif
endif

# tell the source that we are building a dedicated server
ifdef DEDICATED
CDEFS += -DDEDICATED
endif

# SDL config
ifdef WITH_SDL
CDEFS += -DWITH_SDL
CCFLAGS_SDL := $(shell $(SDL_CONFIG) --cflags)
CFLAGS += $(CCFLAGS_SDL)
ifdef STATIC
LDFLAGS_SDL := $(shell $(SDL_CONFIG) --static-libs)
else
LDFLAGS_SDL := $(shell $(SDL_CONFIG) --libs)
endif
LIBS += $(LDFLAGS_SDL)
endif

# zlib config
ifdef WITH_ZLIB
	CDEFS += -DWITH_ZLIB
	ifdef STATIC
		ifdef OSX
		# OSX links dynamically to zlib, even in static builds since it's always present in the system
			LIBS += -lz
		else
			LIBS += $(STATIC_ZLIB_PATH)
		endif
	else
		LIBS += -lz
	endif
endif

# libpng config
ifdef WITH_PNG
CDEFS += -DWITH_PNG
CCFLAGS_PNG := $(shell $(LIBPNG_CONFIG) --cppflags --I_opts)
CFLAGS += $(CCFLAGS_PNG)

# seems like older libpng versions are broken and need this
PNGCONFIG_FLAGS = --ldflags --libs
ifdef STATIC
ifdef OSX
# Seems like we need a tiny hack for OSX static to work
LDFLAGS_PNG := $(shell $(LIBPNG_CONFIG) --prefix)/lib/libpng.a
else
LDFLAGS_PNG := $(shell $(LIBPNG_CONFIG) --static $(PNGCONFIG_FLAGS))
endif
else
LDFLAGS_PNG := $(shell $(LIBPNG_CONFIG) --L_opts $(PNGCONFIG_FLAGS))
endif
LIBS += $(LDFLAGS_PNG)
endif

# use std C++ lib:
LIBS += -lstdc++
ifndef MINGW
	LIBS += -lc
endif

# iconv is enabled defaultly on OSX >= 10.3
ifdef OSX
	ifndef JAGUAR
		WITH_ICONV=1
		LIBS += -liconv
	endif
endif

ifdef WITH_ICONV
	CDEFS += -DWITH_ICONV
	ifdef WITH_ICONV_PATH
		CFLAGS += -I$(WITH_ICONV_PATH)
	endif
endif

# enables/disables assert()
ifdef DISABLE_ASSERTS
CFLAGS += -DNDEBUG
endif

ifdef NO_THREADS
CFLAGS += -DNO_THREADS
endif

# automatically disables asserts for release
ifdef RELEASE
ifndef ENABLE_ASSERTS
CFLAGS += -DNDEBUG
endif
endif

ifdef TRANSLATOR
STRGEN_FLAGS=-t
else
STRGEN_FLAGS=
endif

# OSX specific setup
ifdef OSX
	# set the endian flag for OSX, that can't fail
	ENDIAN_FORCE:=PREPROCESSOR

	# -lrt fails on OSX, so we disable it
	LRT:=

	ifndef DEDICATED
		LIBS += -framework QuickTime
	endif

	ifdef WITH_COCOA
		CDEFS += -DWITH_COCOA
		LIBS += -F/System/Library/Frameworks -framework Cocoa -framework Carbon -framework AudioUnit
	endif

	# OSX path setup
	ifndef SECOND_DATA_PATH
		SECOND_DATA_PATH:="$(OSXAPP)/Contents/Data/"
	endif

	ifndef CUSTOM_LANG_DIR
		ifndef DEDICATED
		CUSTOM_LANG_DIR:="$(OSXAPP)/Contents/Lang/"
		endif
	endif
endif

ifdef MIDI
CDEFS += -DEXTERNAL_PLAYER=\"$(MIDI)\"
ifdef MIDI_ARG
CDEFS += -DMIDI_ARG=\"$(MIDI_ARG)\"
endif
endif

ifdef WITH_NETWORK
CDEFS += -DENABLE_NETWORK
ifdef QNX
LIBS += -lsocket
endif
endif


ifdef SECOND_DATA_PATH
CDEFS += -DSECOND_DATA_DIR=\"$(SECOND_DATA_PATH)/\"
endif

ifdef CUSTOM_LANG_DIR
CDEFS += -DCUSTOM_LANG_DIR=\"$(CUSTOM_LANG_DIR)/\"
endif

ifdef WITH_DIRECTMUSIC
CDEFS += -DWIN32_ENABLE_DIRECTMUSIC_SUPPORT
endif

ifdef WIN32
LIBS += -lws2_32 -lwinmm -lgdi32 -ldxguid -lole32
ifdef WITH_DIRECTMUSIC
LIBS += -lstdc++
endif
TTDLDFLAGS += -Wl,--subsystem,windows
endif

ifndef DEST_DIR
DEST_DIR = $(DESTDIR)
endif

# sets up the paths for use for make install
ifdef INSTALL
# We use _PREFIXED vars here, so the paths are recalculated every time, and
# the prefix is not prepended in the makefile config
BINARY_DIR_PREFIXED:=$(PREFIX)/$(BINARY_DIR)
DATA_DIR_PREFIXED:=$(PREFIX)/$(DATA_DIR)
ICON_DIR_PREFIXED:=$(PREFIX)/$(ICON_DIR)
# We use _INSTALL vars here, these vars are the locations where the files will
# be installed
DATA_DIR_INSTALL=$(DEST_DIR)/$(DATA_DIR_PREFIXED)
BINARY_DIR_INSTALL=$(DEST_DIR)/$(BINARY_DIR_PREFIXED)
ICON_DIR_INSTALL=$(DEST_DIR)/$(ICON_DIR_PREFIXED)
# Let the code know where to find stuff
ifdef DATA_DIR_PREFIXED
CDEFS += -DGAME_DATA_DIR=\"$(DATA_DIR_PREFIXED)/\"
endif

ifdef PERSONAL_DIR
CDEFS += -DPERSONAL_DIR=\"$(PERSONAL_DIR)/\"
endif

ifdef USE_HOMEDIR
CDEFS += -DUSE_HOMEDIR
endif

ifdef ICON_DIR
CDEFS += -DICON_DIR=\"$(ICON_DIR_PREFIXED)/\"
endif
endif

##############################################################################
#
# What to compile
# (users do not want to modify anything below)
#


### Sources

# clean up C_SOURCES first. Needed since building universal binaries on OSX calls the makefile recursively (just one time)
SRCS :=

SRCS += aircraft_cmd.c
SRCS += aircraft_gui.c
SRCS += airport.c
SRCS += airport_gui.c
SRCS += aystar.c
SRCS += bmp.c
SRCS += bridge_gui.c
SRCS += bridge_map.c
SRCS += build_vehicle_gui.c
SRCS += callback_table.c
SRCS += clear_cmd.c
SRCS += command.c
SRCS += console.c
SRCS += console_cmds.c
SRCS += currency.c
SRCS += date.c
SRCS += debug.c
SRCS += dedicated.c
SRCS += depot.c
SRCS += depot_gui.c
SRCS += disaster_cmd.c
SRCS += dock_gui.c
SRCS += driver.c
SRCS += dummy_land.c
SRCS += economy.c
SRCS += elrail.c
SRCS += engine.c
SRCS += engine_gui.c
SRCS += fileio.c
SRCS += fios.c
SRCS += genworld.c
SRCS += genworld_gui.c
SRCS += gfx.c
SRCS += gfxinit.c
SRCS += graph_gui.c
SRCS += heightmap.c
SRCS += industry_cmd.c
SRCS += industry_gui.c
SRCS += intro_gui.c
SRCS += landscape.c
SRCS += main_gui.c
SRCS += map.c
SRCS += md5.c
SRCS += mersenne.c
SRCS += minilzo.c
SRCS += misc.c
SRCS += misc_cmd.c
SRCS += misc_gui.c
SRCS += mixer.c
SRCS += music.c
SRCS += music_gui.c
SRCS += namegen.c
SRCS += network.c
SRCS += network_client.c
SRCS += network_data.c
SRCS += network_gamelist.c
SRCS += network_gui.c
SRCS += network_server.c
SRCS += network_udp.c
SRCS += newgrf.c
SRCS += newgrf_cargo.c
SRCS += newgrf_engine.c
SRCS += newgrf_sound.c
SRCS += newgrf_spritegroup.c
SRCS += newgrf_station.c
SRCS += newgrf_text.c
SRCS += news_gui.c
SRCS += npf.c
SRCS += oldloader.c
SRCS += openttd.c
SRCS += order_cmd.c
SRCS += order_gui.c
SRCS += os_timer.c
SRCS += pathfind.c
SRCS += player_gui.c
SRCS += players.c
SRCS += pool.c
SRCS += queue.c
SRCS += rail.c
SRCS += rail_cmd.c
SRCS += rail_gui.c
SRCS += rev.c
SRCS += road_cmd.c
SRCS += road_gui.c
SRCS += road_map.c
SRCS += roadveh_cmd.c
SRCS += roadveh_gui.c
SRCS += saveload.c
SRCS += screenshot.c
SRCS += settings.c
SRCS += settings_gui.c
SRCS += ship_cmd.c
SRCS += ship_gui.c
SRCS += signs.c
SRCS += smallmap_gui.c
SRCS += sound.c
SRCS += spritecache.c
SRCS += station_cmd.c
SRCS += station_gui.c
SRCS += station_map.c
SRCS += string.c
SRCS += strings.c
SRCS += subsidy_gui.c
SRCS += terraform_gui.c
SRCS += texteff.c
SRCS += tgp.c
SRCS += thread.c
SRCS += tile.c
SRCS += town_cmd.c
SRCS += town_gui.c
SRCS += train_cmd.c
SRCS += train_gui.c
SRCS += tree_cmd.c
SRCS += tunnel_map.c
SRCS += tunnelbridge_cmd.c
SRCS += unmovable_cmd.c
SRCS += vehicle.c
SRCS += vehicle_gui.c
SRCS += viewport.c
SRCS += water_cmd.c
SRCS += waypoint.c
SRCS += widget.c
SRCS += window.c
SRCS += music/null_m.c
SRCS += sound/null_s.c
SRCS += video/dedicated_v.c
SRCS += video/null_v.c
SRCS += yapf/follow_track.cpp
SRCS += yapf/yapf_common.cpp
SRCS += yapf/yapf_rail.cpp
SRCS += yapf/yapf_road.cpp
SRCS += yapf/yapf_ship.cpp

# AI related files
SRCS += ai/ai.c
SRCS += ai/default/default.c
SRCS += ai/trolly/build.c
SRCS += ai/trolly/pathfinder.c
SRCS += ai/trolly/shared.c
SRCS += ai/trolly/trolly.c

ifdef WITH_SDL
  SRCS += sdl.c
  SRCS += sound/sdl_s.c
  SRCS += video/sdl_v.c
endif

ifdef WIN32
  SRCS += win32.c
  SRCS += music/win32_m.c
  SRCS += sound/win32_s.c
  SRCS += video/win32_v.c
else
  SRCS += unix.c
  SRCS += music/extmidi.c
endif

ifdef OSX
	SRCS += os/macosx/macos.m
	ifndef DEDICATED
		SRCS += music/qtmidi.c
	endif
	ifdef WITH_COCOA
		SRCS += video/cocoa_v.m
		SRCS += sound/cocoa_s.c
		SRCS += os/macosx/splash.c
	endif
endif

ifdef BEOS
  SRCS += music/bemidi.cpp
endif

ifdef WIN32
  SRCS += ottdres.rc
endif

ifdef WITH_DIRECTMUSIC
  SRCS += music/dmusic.cpp
endif

OBJS += $(filter %.o, $(SRCS:%.cpp=%.o) $(SRCS:%.m=%.o) $(SRCS:%.c=%.o) $(SRCS:%.rc=%.o))
DEPS = $(OBJS:%.o=.deps/%.d)

LANG_TXT = $(filter-out %.unfinished.txt,$(wildcard lang/*.txt))
LANGS = $(LANG_TXT:%.txt=%.lng)


##############################################################################
#
# Build commands
#

# If we are verbose, we will show commands prefixed by $(Q).
# The $(Q)s get replaced by @ in non-verbose mode.
# Inspired by the Linux kernel build system.
ifdef VERBOSE
	Q =
else
	Q = @
endif


##############################################################################
#
# Targets
#


### Normal build rules


ifdef OSX
# needs to be before all
OSX:=OSX
endif


all: endian_target.h endian_host.h $(UPDATECONFIG) $(LANGS) $(TTD) $(OSX)

ifdef OSX
-include os/macosx/Makefile
endif

endian_host.h: $(ENDIAN_CHECK)
	@echo '===> Testing endianness for host'
	$(Q)./$(ENDIAN_CHECK) > $@

endian_target.h: $(ENDIAN_CHECK)
	@echo '===> Testing endianness for target'
	$(Q)./$(ENDIAN_CHECK) $(ENDIAN_FORCE) > $@

$(ENDIAN_CHECK): endian_check.c
	@echo '===> Compiling and Linking $@'
	$(Q)$(CC_HOST) $(CFLAGS_HOST) $(CDEFS) $< -o $@


ifndef MACOSX_BUILD
# OSX links in os/macosx/Makefile to handle universal binaries better
$(TTD): $(OBJS) $(MAKE_CONFIG)
	@echo '===> Linking $@'
	$(Q)$(CXX_TARGET) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS) $(LIBS) -o $@
endif

$(STRGEN): strgen/strgen.c string.c endian_host.h
	@echo '===> Compiling and Linking $@'
	$(Q)$(CC_HOST) $(CFLAGS_HOST) -DSTRGEN strgen/strgen.c string.c -o $@

table/strings.h: lang/english.txt $(STRGEN)
	@echo '===> Generating $@'
	$(Q)$(STRGEN) -s lang -d table

lang/%.lng: lang/%.txt $(STRGEN) lang/english.txt
	@echo '===> Compiling language $(*F)'
	$(Q)$(STRGEN) $(STRGEN_FLAGS) -s lang -d lang $< $(LANG_ERRORS) || rm -f $@

ifdef MORPHOS

release: all
	$(Q)rm -fr "/t/openttd-$(RELEASE)-morphos.lha"
	$(Q)mkdir -p "/t/"
	$(Q)mkdir -p "/t/openttd-$(RELEASE)-morphos"
	$(Q)mkdir -p "/t/openttd-$(RELEASE)-morphos/docs"
	$(Q)mkdir -p "/t/openttd-$(RELEASE)-morphos/data"
	$(Q)mkdir -p "/t/openttd-$(RELEASE)-morphos/lang"
	$(Q)mkdir -p "/t/openttd-$(RELEASE)-morphos/scenario"
	$(Q)cp -R $(TTD)                      "/t/openttd-$(RELEASE)-morphos/"
	$(Q)cp data/*                         "/t/openttd-$(RELEASE)-morphos/data/"
	$(Q)cp lang/*.lng                     "/t/openttd-$(RELEASE)-morphos/lang/"
	$(Q)cp scenario/*                     "/t/openttd-$(RELEASE)-morphos/scenario/"
	$(Q)cp readme.txt                     "/t/openttd-$(RELEASE)-morphos/docs/ReadMe"
	$(Q)cp docs/console.txt               "/t/openttd-$(RELEASE)-morphos/docs/Console"
	$(Q)cp COPYING                        "/t/openttd-$(RELEASE)-morphos/docs/"
	$(Q)cp changelog.txt                  "/t/openttd-$(RELEASE)-morphos/docs/ChangeLog"
	$(Q)cp known-bugs.txt				   "/t/openttd-$(RELEASE)-morphos/docs/known-bugs.txt"
	$(Q)cp os/morphos/icons/openttd.info  "/t/openttd-$(RELEASE)-morphos/$(TTD).info"
	$(Q)cp os/morphos/icons/docs.info     "/t/openttd-$(RELEASE)-morphos/docs.info"
	$(Q)cp os/morphos/icons/drawer.info   "/t/openttd-$(RELEASE)-morphos.info"
	$(Q)cp os/morphos/icons/document.info "/t/openttd-$(RELEASE)-morphos/docs/ReadMe.info"
	$(Q)cp os/morphos/icons/document.info "/t/openttd-$(RELEASE)-morphos/docs/Console.info"
	$(Q)cp os/morphos/icons/document.info "/t/openttd-$(RELEASE)-morphos/docs/COPYING.info"
	$(Q)cp os/morphos/icons/document.info "/t/openttd-$(RELEASE)-morphos/docs/ChangeLog.info"
	$(Q)strip --strip-all --strip-unneeded --remove-section .comment "/t/openttd-$(RELEASE)-morphos/$(TTD)"
	$(Q)lha a -r "t:openttd-$(RELEASE)-morphos.lha" "t:openttd-$(RELEASE)-morphos"
	$(Q)lha a    "t:openttd-$(RELEASE)-morphos.lha" "t:openttd-$(RELEASE)-morphos.info"
	$(Q)rm -fr "/t/openttd-$(RELEASE)-morphos"
	$(Q)rm -fr "/t/openttd-$(RELEASE)-morphos.info"
	@echo "Release archive can be found in RAM:t/ now."

.PHONY: release
endif

rev.c: FORCE
	@# setting the revision number in a place, there the binary can read it
	@echo 'const char _openttd_revision[] = "$(REV)";' >>rev.c.new
	@# some additions for MorphOS versions tag
	@echo '#ifdef __MORPHOS__'  >>rev.c.new
	@echo 'const char morphos_versions_tag[] = "\\0$$VER: OpenTTD $(REV) ('${BUILDDATE}') © OpenTTD Team [MorphOS, PowerPC]";'  >>rev.c.new
	@echo '#endif' >>rev.c.new
	@# Only update the real rev.c if it actually changed, to prevent
	@# useless rebuilds.
	@cmp -s rev.c rev.c.new 2>/dev/null || mv rev.c.new rev.c
	@rm -f rev.c.new

FORCE:


clean:
	@echo '===> Cleaning up'
# endian.h is out-dated and no longer in use, so it can be removed soon
	$(Q)rm -rf .deps *~ $(TTD) $(STRGEN) core table/strings.h $(LANGS) $(OBJS) $(OSX_MIDI_PLAYER_FILE) endian.h endian_host.h endian_target.h $(ENDIAN_CHECK) .OSX

mrproper: clean
	$(Q)rm -rf $(MAKE_CONFIG)

ifndef OSX
ifndef MORPHOS
install:
ifeq ($(INSTALL),)
	$(error make install is highly experimental at his state and not\
	tested very much - use at your own risk - to use run \"make install INSTALL:=1\" - make sure Makefile.config\
	is set correctly up - run \"make upgradeconf\")
endif

ifeq ($(PREFIX), )
	$(error no prefix set - check Makefile.config)
endif
#	We compare against the non prefixed version here, so we won't install
#	if only the prefix has been set
ifeq ($(DATA_DIR),)
	$(error no data path set - check Makefile.config)
endif
ifeq ($(BINARY_DIR),)
	$(error no binary path set - check Makefile.config)
endif
# We'll install in $DEST_DIR instead of root if it is set (we don't
# care about extra /'s
	install -d $(DATA_DIR_INSTALL)/lang \
	           $(DATA_DIR_INSTALL)/data \
	           $(DATA_DIR_INSTALL)/gm \
						 $(ICON_DIR_INSTALL) \
	           $(BINARY_DIR_INSTALL)
ifndef USE_HOMEDIR
	mkdir -p $(PERSONAL_DIR)/scenario
	mkdir -p $(PERSONAL_DIR)/scenario/heightmap
else
	mkdir -p $(DATA_DIR_INSTALL)/scenario
	mkdir -p $(DATA_DIR_INSTALL)/scenario/heightmap
endif
	install $(TTD) $(BINARY_DIR_INSTALL)
	install -m 644 lang/*.lng $(DATA_DIR_INSTALL)/lang
	install -m 644 data/*.grf $(DATA_DIR_INSTALL)/data
	install -m 644 data/opntitle.dat $(DATA_DIR_INSTALL)/data
	install -m 644 media/openttd.64.png $(ICON_DIR_INSTALL)
	install -m 644 media/openttd.32.xpm $(ICON_DIR_INSTALL)
else	#MorphOS
install:
	$(error make install is not supported on MorphOS)
endif
else	# OSX
install:
	$(error make install is not supported on MacOSX)
endif


love:
	@echo "YES! I thought you would never ask. We will have a great time. You can keep me turned on all night"

.PHONY: clean all $(OSX) install love


### Automatic configuration
-include $(CONFIG_WRITER)


# Export all variables set to subprocesses (a bit dirty)
.EXPORT_ALL_VARIABLES:
upgradeconf: $(MAKE_CONFIG)
	$(Q)rm $(MAKE_CONFIG)
	$(Q)$(MAKE) $(MAKE_CONFIG)

.PHONY: upgradeconf


### Internal build rules

# This makes sure the .deps dir is always around.
DEPS_MAGIC := $(shell mkdir -p $(sort $(dir $(DEPS))))

depend:
	@true # The include handles this automagically

# Introduce the dependencies
ifeq ($(findstring $(MAKECMDGOALS), clean info mrproper upgradeconf $(MAKE_CONFIG)),)
-include $(DEPS)
endif

# Silence stale header dependency errors
%.h:
	@true

.deps/%.d: %.c $(MAKE_CONFIG) table/strings.h endian_target.h
	@echo '===> DEP $<'
	$(Q)$(CC_TARGET) $(CFLAGS) $(CDEFS) -MM $< | sed 's#^$(@F:%.d=%.o):#$@ $(@:.deps/%.d=%.o):#' > $@

.deps/%.d: %.cpp $(MAKE_CONFIG) table/strings.h endian_target.h
	@echo '===> DEP $<'
	$(Q)$(CXX_TARGET) $(CFLAGS) $(CDEFS) -MM $< | sed 's#^$(@F:%.d=%.o):#$@ $(@:.deps/%.d=%.o):#' > $@

.deps/%.d: %.m $(MAKE_CONFIG) table/strings.h endian_target.h
	@echo '===> DEP $<'
	$(Q)$(CC_TARGET) $(OBJCFLAGS) $(CDEFS) -MM $< | sed 's#^$(@F:%.d=%.o):#$@ $(@:.deps/%.d=%.o):#' > $@


ifndef MACOSX_BUILD
# OSX uses os/macosx/Makefile to compile files
%.o: %.c $(MAKE_CONFIG)
	@echo '===> Compiling $<'
	$(Q)$(CC_TARGET) $(CC_CFLAGS) $(CFLAGS) $(CDEFS) -c -o $@ $<

%.o: %.cpp  $(MAKE_CONFIG)
	@echo '===> Compiling $<'
	$(Q)$(CXX_TARGET) $(CFLAGS) $(CDEFS) -c -o $@ $<

%.o: %.m  $(MAKE_CONFIG)
	@echo '===> Compiling $<'
	$(Q)$(CC_TARGET) $(CC_CFLAGS) $(CFLAGS) $(CDEFS) -c -o $@ $<
endif

%.o: %.rc
	@echo '===> Compiling resource $<'
	$(Q)$(WINDRES) -o $@ $<


info:
	@echo 'CFLAGS  = $(CFLAGS)'
	@echo 'LDFLAGS = $(LDFLAGS)'
	@echo 'LIBS    = $(LIBS)'
	@echo 'CDEFS   = $(CDEFS)'
