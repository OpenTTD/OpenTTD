# This Makefile is partially based on "a completely generic Makefile",
# originally created by Justin Husted <husted@cs>
#
# Rewrite and sane dependencies support by Petr Baudis <pasky@ucw.cz>
# Cygwin support and configuration by Jaen Saul <slowbyte@hot.ee>
# A lot of modifications by Bjarni Corfitzen <bjarni@openttd.com>
#
# Last modified by: $Author: strigeus $
# On: $Date: 2004/03/11 19:15:06 $


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
#
# Summary of other defines:
# MANUAL_CONFIG: do not use Makefile.config, config options set manually
# DEBUG: build in debug mode
# PROFILE: build in profile mode, disables -s and -fomit-frame-pointer
# TRANSLATOR: build in translator mode (untranslated strings are prepended by
#             a <TODO> mark)
# RELEASE: this will be the released version number. It replaces all places
#          where it normally would print the revision number
# MIDI: if set, it will use it as custom path to midi player.
#       If unset, it will use the hardcoded path in the c code
#       This can still be overriden by the music.extmidi openttd.cfg option.
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
# DATA_DIR: 	The location of the data (lang, data and gm), normally
#		share/games/openttd. (Will be prefixed with $PREFIX)
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
# ENDIAN_FORCE: forces the endian-check to give a certain result. Can be either BE or LE.
# WINDRES: the location of your windres
# CC_HOST: the gcc of your localhost if you are making a target that produces incompatible executables
# CFLAGS_HOST: cflags used for CC_HOST. Make it something if you are getting errors when you try to compi
#		windows executables on linux. (just: CFLAGS_HOST:='-I' or something)
#
# Experimental (does not work properly):
# WITH_DIRECTMUSIC: enable DirectMusic MIDI support


##############################################################################
#
# Configuration
#


# Makefile version tag
# it checks if the version tag in Makefile.config is the same and force update outdated config files
MAKEFILE_VERSION:=7

# CONFIG_WRITER has to be found even for manual configuration
CONFIG_WRITER=makefiledir/Makefile.config_writer

ifndef MANUAL_CONFIG
# Automatic configuration
MAKE_CONFIG:=Makefile.config
MAKEFILE:=Makefile
LIB_DETECTION=makefiledir/Makefile.libdetection
CONFIG_WRITER=makefiledir/Makefile.config_writer

# Apply automatic configuration
# See target section for how this is built, suppress errors
# since first time it isn't found but make reads this twice
-include $(MAKE_CONFIG)
else
CONFIG_INCLUDED:=1
endif

# updates Makefile.config if it's outdated
ifneq ($(MAKEFILE_VERSION),$(CONFIG_VERSION))
	ifndef MANUAL_CONFIG	# manual config should not check this
		UPDATECONFIG:=upgradeconf
		CONFIG_INCLUDED:=
	else
		# this should define SDL-CONFIG for manual configuration
		ifeq ($(shell uname),FreeBSD)
			SDL-CONFIG:=sdl11-config
		else
			SDL-CONFIG:=sdl-config
		endif
	endif
else
	# this should define SDL-CONFIG for manual configuration
	ifeq ($(shell uname),FreeBSD)
		SDL-CONFIG:=sdl11-config
	else
		SDL-CONFIG:=sdl-config
	endif
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

ifdef RELEASE
ifdef OSX
ifndef STATIC
$(error do not make dynamically linked releases. Most users can't use those)
endif
endif
endif

# Force SDL on UNIX platforms
ifndef WITH_SDL
ifdef UNIX
ifndef DEDICATED
$(error You need to have SDL installed in order to run OpenTTD on UNIX. Use DEDICATED if you want to compile a CLI based server)
endif
endif
endif

# remove the dependancy for sdl if DEDICALTED is used
ifdef DEDICATED
WITH_SDL:=
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

# What revision are we compiling, if we have an idea?
REV_NUMBER := $(shell if test -d .svn; then svnversion . | tr -dc 0-9; fi)

ifdef RELEASE
REV:=$(RELEASE)
else
REV := $(shell if test -d .svn; then svnversion . | awk '{ print "r"$$0 }'; fi)
tmp_test:=$(shell echo "$(REV)" | grep "M" )
ifdef tmp_test
REV_NUMBER:=1
endif
endif

ifndef REV_NUMBER
REV_NUMBER:=0
endif

# MorphOS needs builddate
BUILDDATE=`date +%d.%m.%y`

# AMD64 needs a little more settings to work
ifeq ($(shell uname -m), x86_64)
endwarnings:=endwarnings
64_bit_warnings:=64_bit_warnings
BASECFLAGS += -m64
endif

# Check if there is a windres override
ifndef WINDRES
WINDRES = windres
endif

# Check if we have a new target
ifdef CC_TARGET
CC = $(CC_TARGET)
endif

# Check if CC_HOST is defined. If not, it is CC
ifndef CC_HOST
CC_HOST = $(CC)
endif
ifndef CFLAGS_HOST
CFLAGS_HOST = $(BASECFLAGS)
endif

# When calling the compiler, use these flags
# -g	debugging symbols
# -Wall	all warnings
# -s    automatic strip
#
# You may also want:
# -O	optimize or -O2 fully optimize (O's above 2 are not recommended)
# -pg	profile - generate profiling data.  See "man gprof" to use this.

CC_VERSION = $(shell $(CC) -dumpversion | cut -c 1,3)

# GNU make can only test for (in)equality
# this is a workaround to test for >=
ifeq ($(shell if test $(CC_VERSION) -ge 29; then echo true; fi), true)
  CFLAGS += -O -Wall -Wno-multichar -Wsign-compare -Wstrict-prototypes
  CFLAGS += -Wwrite-strings -Wpointer-arith
endif
ifeq ($(shell if test $(CC_VERSION) -ge 30; then echo true; fi), true)
  CFLAGS += -W -Wno-unused-parameter
endif
ifeq ($(shell if test $(CC_VERSION) -ge 34; then echo true; fi), true)
  CFLAGS += -Wdeclaration-after-statement -Wold-style-definition
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
endif

CDEFS=-DWITH_REV

ifndef DEBUG
ifndef PROFILE
# Release mode
ifndef MORPHOS
# automatical strip breaks under morphos
BASECFLAGS += -s
LDFLAGS += -s
endif
endif

ifdef OSX
# these compilerflags makes the app run as fast as possible without making the app unstable. It works on G3 or newer
BASECFLAGS += -O3 -funroll-loops -fsched-interblock -falign-loops=16 -falign-jumps=16 -falign-functions=16 -falign-jumps-max-skip=15 -falign-loops-max-skip=15 -mdynamic-no-pic -mpowerpc-gpopt -force_cpusubtype_ALL
else
ifdef MORPHOS
BASECFLAGS += -O2 -noixemul -fstrict-aliasing -fexpensive-optimizations
BASECFLAGS += -mcpu=604 -fno-inline -mstring -mmultiple
else
BASECFLAGS += -O2
endif
ifndef PROFILE
BASECFLAGS += -fomit-frame-pointer
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
endif
endif

CFLAGS += $(BASECFLAGS)

ifdef UNIX
CDEFS += -DUNIX
endif

ifdef BEOS
CDEFS += -DBEOS
LDFLAGS += -lmidi -lbe
ifdef WITH_NETWORK
	ifdef BEOS_NET_SERVER
		CDEFS += -DBEOS_NET_SERVER
	else
		# Zeta needs a few more libraries than R5
		LDFLAGS += -lbind -lsocket
	endif
endif
endif

ifdef MORPHOS
# -Wstrict-prototypes generates much noise because of system headers
CFLAGS += -Wno-strict-prototypes
endif

ifdef SUNOS
CDEFS += -DSUNOS
ifdef WITH_NETWORK
LDFLAGS += -lnsl -lsocket
endif
endif

# SDL config
ifdef WITH_SDL
CDEFS += -DWITH_SDL
CFLAGS += $(shell $(SDL-CONFIG) --cflags)
ifdef STATIC
LIBS += $(shell $(SDL-CONFIG) --static-libs)
else
LIBS += $(shell $(SDL-CONFIG) --libs)
endif
endif


# zlib config
ifdef WITH_ZLIB
	CDEFS +=  -DWITH_ZLIB
	ifdef STATIC
		ifdef OSX
# zlib is default on OSX, so everybody have it. No need for static linking
			LIBS += -lz
		else
			ifndef STATIC_ZLIB_PATH
				ifndef MANUAL_CONFIG
					# updates Makefile.config with the zlib path
					UPDATECONFIG:=upgradeconf
				endif
				TEMP:=$(shell ls /lib 2>/dev/null | grep "zlib.a")$(shell ls /lib 2>/dev/null | grep "libz.a")
				ifdef TEMP
					STATIC_ZLIB_PATH:=/lib/$(TEMP)
				else
					TEMP:=$(shell ls /usr/lib 2>/dev/null | grep "zlib.a")$(shell ls /usr/lib 2>/dev/null | grep "libz.a")
					ifdef TEMP
						STATIC_ZLIB_PATH:=/usr/lib/$(TEMP)
					else
						TEMP:=$(shell ls /usr/local/lib 2>/dev/null | grep "zlib.a")$(shell ls /usr/local/lib 2>/dev/null | grep "libz.a")
						ifdef TEMP
							STATIC_ZLIB_PATH:=/usr/local/lib/$(TEMP)
						endif
					endif
				endif
			endif
			LIBS += $(STATIC_ZLIB_PATH)
		endif
	else
		LIBS += -lz
	endif
endif

# libpng config
ifdef WITH_PNG
CDEFS += -DWITH_PNG
# FreeBSD doesn't use libpng-config
ifdef FREEBSD
LIBS += -lpng
else
CFLAGS += $(shell libpng-config --cflags)

# seems like older libpng versions are broken and need this
PNGCONFIG_FLAGS = --ldflags --libs
ifdef STATIC
ifdef OSX
# Seems like we need a tiny hack for OSX static to work
LIBS += $(shell libpng-config --prefix)/lib/libpng.a
else
LIBS += $(shell libpng-config --static $(PNGCONFIG_FLAGS))
endif
else
LIBS += $(shell libpng-config  --L_opts $(PNGCONFIG_FLAGS))
endif
endif
endif

# enables/disables assert()
ifdef DISABLE_ASSERTS
CFLAGS += -DNDEBUG
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


# MIDI setup
ifdef OSX
ifndef MIDI
MIDI:=$(OSXAPP)/contents/macos/track_starter
endif
ifndef SECOND_DATA_PATH
SECOND_DATA_PATH:="$(OSXAPP)/contents/data/"
endif
ifndef CUSTOM_LANG_DIR
CUSTOM_LANG_DIR:="$(OSXAPP)/contents/lang/"
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
ifdef UNIX
ifndef OSX
ifndef MORPHOS
# this have caused problems on many platforms and disabling it didn't break anything
# now we test if disabling it as a general breaks it for anybody
#LIBS += -lresolv
endif
endif
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
# We use _INSTALL vars here, these vars are the locations where the files will
# be installed
DATA_DIR_INSTALL=$(DEST_DIR)/$(DATA_DIR_PREFIXED)
BINARY_DIR_INSTALL=$(DEST_DIR)/$(BINARY_DIR_PREFIXED)
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
endif

##############################################################################
#
# What to compile
# (users do not want to modify anything below)
#


### Sources

C_SOURCES += ai.c
C_SOURCES += ai_build.c
C_SOURCES += ai_new.c
C_SOURCES += ai_pathfinder.c
C_SOURCES += ai_shared.c
C_SOURCES += aircraft_cmd.c
C_SOURCES += aircraft_gui.c
C_SOURCES += airport.c
C_SOURCES += airport_gui.c
C_SOURCES += aystar.c
C_SOURCES += bridge_gui.c
C_SOURCES += callback_table.c
C_SOURCES += clear_cmd.c
C_SOURCES += command.c
C_SOURCES += console.c
C_SOURCES += console_cmds.c
C_SOURCES += debug.c
C_SOURCES += dedicated.c
C_SOURCES += depot.c
C_SOURCES += disaster_cmd.c
C_SOURCES += dock_gui.c
C_SOURCES += dummy_land.c
C_SOURCES += economy.c
C_SOURCES += engine.c
C_SOURCES += engine_gui.c
C_SOURCES += fileio.c
C_SOURCES += gfx.c
C_SOURCES += graph_gui.c
C_SOURCES += newgrf.c
C_SOURCES += industry_cmd.c
C_SOURCES += industry_gui.c
C_SOURCES += intro_gui.c
C_SOURCES += landscape.c
C_SOURCES += main_gui.c
C_SOURCES += map.c
C_SOURCES += md5.c
C_SOURCES += minilzo.c
C_SOURCES += misc.c
C_SOURCES += misc_cmd.c
C_SOURCES += misc_gui.c
C_SOURCES += mixer.c
C_SOURCES += music_gui.c
C_SOURCES += namegen.c
C_SOURCES += network.c
C_SOURCES += network_client.c
C_SOURCES += network_data.c
C_SOURCES += network_gamelist.c
C_SOURCES += network_gui.c
C_SOURCES += network_server.c
C_SOURCES += network_udp.c
C_SOURCES += news_gui.c
C_SOURCES += npf.c
C_SOURCES += oldloader.c
C_SOURCES += order_cmd.c
C_SOURCES += order_gui.c
C_SOURCES += pathfind.c
C_SOURCES += player_gui.c
C_SOURCES += players.c
C_SOURCES += pool.c
C_SOURCES += queue.c
C_SOURCES += rail_cmd.c
C_SOURCES += rail_gui.c
C_SOURCES += rev.c
C_SOURCES += road_cmd.c
C_SOURCES += road_gui.c
C_SOURCES += roadveh_cmd.c
C_SOURCES += roadveh_gui.c
C_SOURCES += saveload.c
C_SOURCES += screenshot.c
C_SOURCES += settings.c
C_SOURCES += settings_gui.c
C_SOURCES += ship_cmd.c
C_SOURCES += ship_gui.c
C_SOURCES += signs.c
C_SOURCES += smallmap_gui.c
C_SOURCES += sound.c
C_SOURCES += sprite.c
C_SOURCES += spritecache.c
C_SOURCES += station_cmd.c
C_SOURCES += station_gui.c
C_SOURCES += string.c
C_SOURCES += strings.c
C_SOURCES += subsidy_gui.c
C_SOURCES += terraform_gui.c
C_SOURCES += texteff.c
C_SOURCES += tile.c
C_SOURCES += town_cmd.c
C_SOURCES += town_gui.c
C_SOURCES += train_cmd.c
C_SOURCES += train_gui.c
C_SOURCES += tree_cmd.c
C_SOURCES += ttd.c
C_SOURCES += tunnelbridge_cmd.c
C_SOURCES += unmovable_cmd.c
C_SOURCES += vehicle.c
C_SOURCES += vehicle_gui.c
C_SOURCES += viewport.c
C_SOURCES += water_cmd.c
C_SOURCES += waypoint.c
C_SOURCES += widget.c
C_SOURCES += window.c

CXX_SOURCES =

ifdef WITH_SDL
C_SOURCES += sdl.c
endif

ifdef WIN32
C_SOURCES += win32.c w32dm.c
else
C_SOURCES += extmidi.c unix.c
endif

OBJS = $(C_SOURCES:%.c=%.o) $(CXX_SOURCES:%.cpp=%.o)

ifdef BEOS
CXX_SOURCES += os/beos/bemidi.cpp
CFLAGS += -I.
endif

ifdef WIN32
# Resource file
OBJS += winres.o
endif

ifdef WITH_DIRECTMUSIC
CXX_SOURCES += w32dm2.cpp
endif

DEPS = $(OBJS:%.o=.deps/%.d)

LANG_TXT = $(filter-out %.unfinished.txt,$(wildcard lang/*.txt))
LANGS = $(LANG_TXT:%.txt=%.lng)


##############################################################################
#
# Build commands
#

# If we are verbose, we will show commands prefixed by $(Q) (which acts as
# @ in the non-verbose mode), and we will show the "real" cmds instead of
# their quiet versions (which are used in the non-verbose mode).
# Inspired by the Linux kernel build system.
ifdef VERBOSE
	Q =
	quiet =
else
	Q = @
	quiet = quiet_
endif

# Show the command (quiet or non-quiet version based on the assignment
# just above) and then execute it.
cmd = @$(if $($(quiet)cmd_$(1)),echo $($(quiet)cmd_$(1)) &&) $(cmd_$(1))


# The build commands themselves. Note that if you omit the quiet version,
# nothing will be shown in the non-verbose mode.

quiet_cmd_compile_link = '===> Compiling and Linking $@'
      cmd_compile_link = $(CC_HOST) $(CFLAGS_HOST) $(CDEFS) $< -o $@

quiet_cmd_ttd_link = '===> Linking $@'
      cmd_ttd_link = $(CC) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS) $(LIBS) -o $@

COMPILE_PARAMS=$(CFLAGS) $(CDEFS) -MD -c $< -o $@

quiet_cmd_c_compile = '===> Compiling $<'
      cmd_c_compile = $(CC) $(COMPILE_PARAMS)

quiet_cmd_cxx_compile = '===> Compiling $<'
      cmd_cxx_compile = $(CXX) $(COMPILE_PARAMS)


##############################################################################
#
# Targets
#


### Normal build rules


ifdef OSX
OSX:=OSX
endif


all: endian.h $(UPDATECONFIG) $(LANGS) $(TTD) $(OSX) $(endwarnings)

endian.h: $(ENDIAN_CHECK)
	@echo '===> Testing endianness'
	$(Q)./$(ENDIAN_CHECK) $(ENDIAN_FORCE) > $@

$(ENDIAN_CHECK): endian_check.c
	$(call cmd,compile_link)


$(TTD): table/strings.h $(OBJS) $(MAKE_CONFIG)
	$(call cmd,ttd_link)

$(OSX): $(TTD)
	$(Q)rm -fr "$(OSXAPP)"
	$(Q)mkdir -p "$(OSXAPP)"/Contents/MacOS
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Resources
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Data
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Lang
	$(Q)echo "APPL????" > "$(OSXAPP)"/Contents/PkgInfo
	$(Q)cp os/macos/openttd.icns "$(OSXAPP)"/Contents/Resources/openttd.icns
	$(Q)os/macos/plistgen.sh "$(OSXAPP)" "$(REV)"
	$(Q)cp os/macos/track_starter "$(OSXAPP)"/contents/macos
	$(Q)ls os/macos | grep -q "\.class" || \
	$(Q)       javac os/macos/OpenTTDMidi.java
	$(Q)cp os/macos/OpenTTDMidi.class "$(OSXAPP)"/contents/macos
	$(Q)cp data/* "$(OSXAPP)"/Contents/data/
	$(Q)cp lang/*.lng "$(OSXAPP)"/Contents/lang/
	$(Q)cp $(TTD) "$(OSXAPP)"/Contents/MacOS/$(TTD)

$(endwarnings): $(64_bit_warnings)

$(64_bit_warnings):
	$(warning 64 bit CPUs will get some 64 bit specific bugs!)
	$(warning If you see any bugs, include in your bug report that you use a 64 bit CPU)

$(STRGEN): strgen/strgen.c endian.h
	$(call cmd,compile_link)

table/strings.h: lang/english.txt $(STRGEN)
	@echo '===> Generating $@'
	$(Q)$(STRGEN)

lang/%.lng: lang/%.txt $(STRGEN) lang/english.txt
	@echo '===> Compiling language $(*F)'
	$(Q)$(STRGEN) $(STRGEN_FLAGS) $< $(LANG_ERRORS)

winres.o: ttd.rc
	@echo '===> Compiling resource $<'
	$(Q)$(WINDRES) -o $@ $<

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

ifdef OSX
release: all
	$(Q)mkdir -p "OpenTTD $(RELEASE)"
	$(Q)mkdir -p "OpenTTD $(RELEASE)"/docs
	$(Q)mkdir -p "OpenTTD $(RELEASE)"/scenario
	$(Q)cp -R $(OSXAPP) "OpenTTD $(RELEASE)"/
	$(Q)cp docs/OSX_where_did_the_package_go.txt "OpenTTD $(RELEASE)"/Where\ did\ the\ package\ go.txt
	$(Q)cp readme.txt "OpenTTD $(RELEASE)"/docs/
	$(Q)cp docs/README_if_game_crashed_on_OSX.txt "OpenTTD $(RELEASE)"/docs/readme\ if\ crashed\ on\ OSX.txt
	$(Q)cp docs/console.txt "OpenTTD $(RELEASE)"/docs/
	$(Q)cp COPYING "OpenTTD $(RELEASE)"/docs/
	$(Q)cp changelog.txt "OpenTTD $(RELEASE)"/docs/
	$(Q)cp docs/README_if_game_crashed_on_OSX.txt "OpenTTD $(RELEASE)"/docs/
	$(Q)cp os/macos/*.webloc "OpenTTD $(RELEASE)"
	$(Q)cp known-bugs.txt "OpenTTD $(RELEASE)"/known-bugs.txt
	$(Q)cp scenario/* "OpenTTD $(RELEASE)"/scenario/
	$(Q)/usr/bin/hdiutil create -ov -format UDZO -srcfolder "OpenTTD $(RELEASE)" openttd-"$(RELEASE)"-osx.dmg
	$(Q)rm -fr "OpenTTD $(RELEASE)"

nightly_build: all
	$(Q)mkdir -p "OpenTTD_nightly_$(DATE)"
	$(Q)mkdir -p "OpenTTD_nightly_$(DATE)"/docs
	$(Q)cp -R $(OSXAPP) "OpenTTD_nightly_$(DATE)"/
	$(Q)cp docs/OSX_where_did_the_package_go.txt "OpenTTD_nightly_$(DATE)"/Where\ did\ the\ package\ go.txt
	$(Q)cp readme.txt "OpenTTD_nightly_$(DATE)"/docs/
	$(Q)cp docs/README_if_game_crashed_on_OSX.txt "OpenTTD_nightly_$(DATE)"/docs/readme\ if\ crashed\ on\ OSX.txt
	$(Q)cp docs/console.txt "OpenTTD_nightly_$(DATE)"/docs/
	$(Q)cp COPYING "OpenTTD_nightly_$(DATE)"/docs/
	$(Q)cp revisionlog.txt "OpenTTD_nightly_$(DATE)"/revisionlog.txt
	$(Q)cp docs/README_if_game_crashed_on_OSX.txt "OpenTTD_nightly_$(DATE)"/docs/
	$(Q)cp os/macos/*.webloc "OpenTTD_nightly_$(DATE)"/
	$(Q)/usr/bin/hdiutil create -ov -format UDZO -srcfolder "OpenTTD_nightly_$(DATE)" openttd-nightly-"$(DATE)".dmg
	$(Q)rm -fr "OpenTTD_nightly_$(DATE)"

.PHONY: release nightly_build
endif

rev.c: FORCE
	@# setting the revision number in a place, there the binary can read it
	@echo 'const char _openttd_revision[] = "$(REV)";' >>rev.c.new
	@echo 'const int _revision_number = $(REV_NUMBER);' >>rev.c.new
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
	$(Q)rm -rf .deps *~ $(TTD) $(STRGEN) core table/strings.h $(LANGS) $(OBJS) endian.h $(ENDIAN_CHECK)

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
	           $(BINARY_DIR_INSTALL)
	mkdir -p   $(PERSONAL_DIR)/scenario
	install $(TTD) $(BINARY_DIR_INSTALL)
	install -m 644 lang/*.lng $(DATA_DIR_INSTALL)/lang
	install -m 644 data/*.grf $(DATA_DIR_INSTALL)/data
	install -m 644 data/opntitle.dat $(DATA_DIR_INSTALL)/data
	install -m 644 media/openttd.64.png $(DATA_DIR_INSTALL)
	cp scenario/* $(PERSONAL_DIR)/scenario/
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

.PHONY: clean all $(OSX) install $(64_bit_warnings) $(endwarnings) love


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
DEPS_MAGIC := $(shell mkdir .deps > /dev/null 2>&1 || :)

# Introduce the dependencies
-include $(DEPS)

# This compiles the object file as well as silently updating its dependencies
# list at the same time. It is not an issue that they aren't around during the
# first compilation round as we just build everything at that time anyway,
# therefore we do not need to watch deps.

%.o: %.c $(MAKE_CONFIG) endian.h table/strings.h
	$(call cmd,c_compile)
	@mv $(<:%.c=%.d) $(<:%.c=.deps/%.d)

%.o: %.cpp  $(MAKE_CONFIG) endian.h table/strings.h
	$(call cmd,cxx_compile)
	@mv $(<:%.cpp=%.d) $(<:%.cpp=.deps/%.d)

# Silence stale header dependencies
%.h:
	@true


info:
	@echo 'CFLAGS  = $(CFLAGS)'
	@echo 'LDFLAGS = $(LDFLAGS)'
	@echo 'LIBS    = $(LIBS)'
