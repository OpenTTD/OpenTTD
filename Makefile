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

# Options:
#
# Summary of OS choice defines
# WIN32: building on Windows
# UNIX: building on *nix derivate (Linux, FreeBSD)
# OSX: building on Mac OS X
# MORPHOS: building on MorphOS
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
# DISPLAY_WARNINGS: when off, some errors are not displayed while compiling
# TRANSLATOR: build in translator mode (untranslated strings are prepended by
#             a <TODO> mark)
# RELEASE: this will be the released version number. It replaces all places
#          where it normally would print the revision number
# MIDI: if set, it will use it as custom path to midi player. 
#  If unset, it will use the hardcoded path in the c code
#
# DATA_DIR_PREFIX: This sets the dir OpenTTD looks for the needed files. 
#   MUST END WITH / if defined
#
# STATIC: link statically
# CYGWIN: build in Cygwin environment
# MINGW: build with MingW compiler, link with MingW libraries
#
# Experimental (does not work properly):
# WITH_NETWORK: enable networking
# WITH_DIRECTMUSIC: enable DirectMusic MIDI support


##############################################################################
#
# Configuration
#

# CONFIG_WRITER have to be found even for manual configuration
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

# tests if makefile.config contains the new needed SDL-CONFIG
# it updates makefile.config if needed. Flags like ENABLE_NETWORK are remembered
ifndef SDL-CONFIG
ifdef WITH_SDL
ifndef MANUAL_CONFIG
#network is enabled by default
ENABLE_NETWORK:=1
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
endif
endif

ifndef CONFIG_INCLUDED
-include $(LIB_DETECTION)
endif

ifdef DISPLAY_WARNINGS
WARNING_DISPLAY:=-fstrict-aliasing
else
WARNING_DISPLAY:=-fno-strict-aliasing
endif

ifdef STATIC
ifndef WIN32
ifndef OSX
ifndef MORPHOS
ifndef SKIP_STATIC_CHECK
$(error Static is only known to work on MorphOS and MacOSX!!! --- Check makefile.config for more info and howto bypass this check)
endif
endif
endif
endif
endif


# Force SDL on UNIX platforms
ifndef WITH_SDL
ifdef UNIX
$(error You need to have SDL installed in order to run OpenTTD on UNIX.)
endif
endif



##############################################################################
#
# Compiler configuration
#

CC=gcc
CXX=g++

ifdef MORPHOS
CC += -noixemul -pipe
CXX += -noixemul -pipe
endif

# Executable file extension
ifdef WIN32
EXE=.exe
else
EXE=
endif

# Set output executable names
TTD=ttd$(EXE)
STRGEN=strgen/strgen$(EXE)
OSXAPP="OpenTTD.app"

# What revision are we compiling, if we have an idea?
REV_NUMBER := $(shell if test -d .svn; then svnversion . | tr -dc 0-9; fi)

ifdef RELEASE
REV:=$(RELEASE)
else
REV := $(shell if test -d .svn; then echo -n r; svnversion .; fi)
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
BASECFLAGS += -m64 -D_LITTLE_ENDIAN
endif


# When calling the compiler, use these flags
# -g	debugging symbols
# -Wall	all warnings
# -s    automatic strip
#
# You may also want:
# -O	optimize or -O2 fully optimize (O's above 2 are not recommended)
# -pg	profile - generate profiling data.  See "man gprof" to use this.

CFLAGS=-Wall -Wno-multichar
CDEFS=-DWITH_REV
LDFLAGS=
LIBS=

ifdef DEBUG
# Debug mode
CDEFS += -D_DEBUG
BASECFLAGS += -g
else
ifdef PROFILE
BASECFLAGS += -pg
else
# Release mode
ifndef MORPHOS
# automatical strip breaks under morphos
BASECFLAGS += -s
LDFLAGS += -s
endif
endif

ifdef OSX
# these compilerflags makes the app run as fast as possible without making the app unstable. It works on G3 or newer
BASECFLAGS += -O3 -funroll-loops -fsched-interblock -falign-loops=16 -falign-jumps=16 -falign-functions=16 -falign-jumps-max-skip=15 -falign-loops-max-skip=15 -mdynamic-no-pic -mpowerpc-gpopt -force_cpusubtype_ALL $(WARNING_DISPLAY)
else
ifdef MORPHOS
BASECFLAGS += -O2 -funroll-loops -fexpensive-optimizations -mstring -mmultiple $(WARNING_DISPLAY)
else
BASECFLAGS += -O2 $(WARNING_DISPLAY)
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

# If building on Cygwin/MingW don't link with Cygwin libs
ifdef WIN32
ifdef MINGW
ifdef CYGWIN
BASECFLAGS += -mno-cygwin
LDFLAGS += -mno-cygwin
endif
endif
endif

CFLAGS += $(BASECFLAGS)

ifdef UNIX
CDEFS += -DUNIX
endif

# SDL config
ifdef WITH_SDL
CDEFS += -DWITH_SDL
CFLAGS += `$(SDL-CONFIG) --cflags`
ifdef STATIC
LIBS += `$(SDL-CONFIG) --static-libs`
else
LIBS += `$(SDL-CONFIG) --libs`
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
					# updates makefile.config with the zlib path
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
CFLAGS += `libpng-config --cflags`
ifdef OSX
ifdef STATIC
# Seems like we need a tiny hack for OSX static to work
LIBS += `libpng-config --prefix`/lib/libpng.a
else
LIBS += `libpng-config  --libs`
endif
else
# seems like older libpng versions are broken and need this
PNGCONFIG_FLAGS = --ldflags --libs
ifdef STATIC
LIBS += `libpng-config --static $(PNGCONFIG_FLAGS)`
else
LIBS += `libpng-config  $(PNGCONFIG_FLAGS)`
endif
endif
endif
endif


ifdef TRANSLATOR
STRGEN_FLAGS=-t
else
STRGEN_FLAGS=
endif

# file paths setup
ifdef GAME_DATA_DIR
CDEFS += -DGAME_DATA_DIR=\"$(GAME_DATA_DIR)\"
endif

ifdef PERSONAL_DIR
CDEFS += -DPERSONAL_DIR=\"$(PERSONAL_DIR)\"
endif

ifdef USE_HOMEDIR
CDEFS += -DUSE_HOMEDIR
endif

# MIDI setup
ifdef OSX
ifndef MIDI
MIDI:=$(OSXAPP)/contents/macos/track_starter
endif
endif

ifdef MIDI
CDEFS += -DEXTERNAL_PLAYER=\"$(MIDI)\"
ifdef MIDI_ARG
CDEFS += -DMIDI_ARG=\"$(MIDI_ARG)\"
endif
endif

# Experimental
ifdef WITH_NETWORK
CDEFS += -DENABLE_NETWORK
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

ifdef WITH_DIRECTMUSIC
CDEFS += -DWIN32_ENABLE_DIRECTMUSIC_SUPPORT
endif

ifdef WIN32
LIBS += -lws2_32 -lwinmm -lgdi32 -ldxguid -lole32 -lstdc++
TTDLDFLAGS += -Wl,--subsystem,windows
endif

# sets up the paths for use for make install
ifdef BINARY_DIR
BINARY_INSTALL:=$(BINARY_DIR)$(TTD)
else
BINARY_INSTALL:=$(INSTALL_DIR)$(TTD)
endif
ifdef DATA_DIR_PREFIX
DATA_DIR:=$(DATA_DIR_PREFIX)
else
DATA_DIR:=$(INSTALL_DIR)
endif

##############################################################################
#
# What to compile
# (users do not want to modify anything below)
#


### Sources

ttd_SOURCES = \
	ai.c aircraft_cmd.c aircraft_gui.c airport.c airport_gui.c bridge_gui.c \
	clear_cmd.c command.c disaster_cmd.c dock_gui.c dummy_land.c economy.c \
	engine.c engine_gui.c fileio.c gfx.c graph_gui.c industry_cmd.c \
	industry_gui.c intro_gui.c landscape.c main_gui.c misc.c misc_cmd.c \
	misc_gui.c music_gui.c namegen.c network.c news_gui.c oldloader.c \
	order_cmd.c order_gui.c pathfind.c player_gui.c players.c rail_cmd.c \
	rail_gui.c road_cmd.c road_gui.c roadveh_cmd.c roadveh_gui.c saveload.c \
	settings_gui.c ship_cmd.c ship_gui.c smallmap_gui.c sound.c \
	spritecache.c station_cmd.c station_gui.c strings.c subsidy_gui.c \
	texteff.c town_cmd.c town_gui.c train_cmd.c train_gui.c tree_cmd.c \
	ttd.c tunnelbridge_cmd.c unmovable_cmd.c vehicle.c viewport.c \
	water_cmd.c widget.c window.c minilzo.c screenshot.c settings.c rev.c \
	grfspecial.c terraform_gui.c network_gui.c

ifdef WITH_SDL
ttd_SOURCES += sdl.c
endif

ifdef WIN32
ttd_SOURCES += win32.c w32dm.c
else
ttd_SOURCES += extmidi.c unix.c
endif

ttd_OBJS = $(ttd_SOURCES:%.c=%.o)

ifdef WIN32
# Resource file
ttd_OBJS += winres.o
endif

ifdef WITH_DIRECTMUSIC
ttd_SOURCES += w32dm2.cpp
ttd_OBJS += w32dm2.o
endif

ttd_DEPS1 = $(foreach obj,$(ttd_OBJS),.deps/$(obj))
ttd_DEPS = $(ttd_DEPS1:%.o=%.P)

LANG_TXT = $(filter-out %.unfinished.txt,$(wildcard lang/*.txt))
LANGS = $(LANG_TXT:%.txt=%.lng)

C_COMPILE = $(CC) $(CFLAGS) $(CDEFS)
CXX_COMPILE = $(CXX) $(CFLAGS) $(CDEFS)

C_BUILD = $(C_COMPILE) -c
CXX_BUILD = $(CXX_COMPILE) -c

C_LINK = $(CC) $(LDFLAGS) -o


##############################################################################
#
# Targets
#


### Normal build rules


ifdef OSX
OSX:=OSX
endif


all: $(UPDATECONFIG) $(TTD) $(OSX) $(endwarnings)


$(TTD): table/strings.h $(ttd_OBJS) $(LANGS) $(MAKE_CONFIG)
	$(C_LINK) $@ $(TTDLDFLAGS) $(ttd_OBJS) $(LIBS)

$(OSX):
	@mkdir -p $(OSXAPP)/Contents/MacOS
	@mkdir -p $(OSXAPP)/Contents/Resources
	@echo "APPL????" > $(OSXAPP)/Contents/PkgInfo
	@cp os/macos/ttd.icns $(OSXAPP)/Contents/Resources/
	@os/macos/plistgen.sh $(OSXAPP) $(REV)
	@cp os/macos/track_starter $(OSXAPP)/contents/macos
	@ls os/macos | grep -q "\.class" || \
	javac os/macos/OpenTTDMidi.java
	@cp os/macos/OpenTTDMidi.class $(OSXAPP)/contents/macos
	@cp $(TTD) $(OSXAPP)/Contents/MacOS/ttd

$(endwarnings): $(64_bit_warnings)

$(64_bit_warnings):
	$(warning 64 bit CPUs will get some 64 bit specific bugs!)
	$(warning If you see any bugs, include in your bug report that you use a 64 bit CPU)

$(STRGEN): strgen/strgen.c rev.o
	$(CC) $(BASECFLAGS) $(CDEFS) -o $@ $^

lang/english.lng: lang/english.txt $(STRGEN)
	$(STRGEN)
	
table/strings.h: lang/english.lng

lang/%.lng: lang/%.txt $(STRGEN)
	$(STRGEN) $(STRGEN_FLAGS) $<

winres.o: ttd.rc
	windres -o $@ $<


rev.c: FORCE
	@# setting the revision number in a place, there the binary can read it
	@echo 'const char _openttd_revision[] = "'$(REV)'";' >>rev.c.new
	@echo 'const int _revision_number = $(REV_NUMBER);' >>rev.c.new
	@# some additions for MorphOS versions tag
	@echo '#ifdef __MORPHOS__'  >>rev.c.new
	@echo 'const char morphos_versions_tag[] = "\\0$$VER: OpenTTD '$(REV)' ('${BUILDDATE}') © OpenTTD Team [MorphOS, PowerPC]";'  >>rev.c.new
	@echo '#endif' >>rev.c.new
	@# Only update the real rev.c if it actually changed, to prevent
	@# useless rebuilds.
	@cmp -s rev.c rev.c.new 2>/dev/null || mv rev.c.new rev.c
	@rm -f rev.c.new

FORCE:


clean:
	rm -rf .deps *~ $(TTD) $(STRGEN) core table/strings.h $(LANGS) $(ttd_OBJS)

mrproper: clean
	rm -rf $(MAKE_CONFIG)

ifndef OSX
ifndef MORPHOS
install:
	@if [ "$(INSTALL)" == "" ]; then $(error make install is highly experimental at his state and not\
	tested very much - use at your own risk - to use run \"make install INSTALL:=1\" - make sure makefile.config\
	is set correctly up - run \"make upgradeconf\")
	@if [ "$(DATA_DIR)" == "" ]; then $(error no install path set - check makefile.config)
	mkdir -p $(DATA_DIR)/lang
	mkdir -p $(DATA_DIR)/data
	cp $(TTD) $(BINARY_INSTALL)
	cp lang/*.lng $(DATA_DIR)/lang
	cp data/*.grf $(DATA_DIR)/data
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
	rm $(MAKE_CONFIG)
	$(MAKE) $(MAKE_CONFIG)

.PHONY: upgradeconf


### Internal build rules

# This makes sure the .deps dir is always around.
DEPS_MAGIC := $(shell mkdir .deps > /dev/null 2>&1 || :)

# Introduce the dependencies
-include $(ttd_DEPS)

# This compiles the object file as well as silently updating its dependencies
# list at the same time. It is not an issue that they aren't around during the
# first compilation round as we just build everything at that time anyway,
# therefore we do not need to watch deps.
%.o: %.c $(MAKE_CONFIG)
	@echo '$(C_BUILD) $<'; \
		$(C_BUILD) $< -Wp,-MD,.deps/$(*F).pp
	@-cp .deps/$(*F).pp .deps/$(*F).P; \
		tr ' ' '\012' < .deps/$(*F).pp \
		| sed -e 's/^\\$$//' -e '/^$$/ d' -e '/:$$/ d' -e 's/$$/ :/' \
		>> .deps/$(*F).P; \
	rm .deps/$(*F).pp

# For DirectMusic build
%.o: %.cpp  $(MAKE_CONFIG)
	$(CXX_BUILD) $<
