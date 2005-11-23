# $Id: Makefile 3214 2005-11-17 19:43:37Z bjarni $
# This makefile is not a standalone makefile, but is called from the general one
# it contains targets specific to MacOS X

# it's possible to link OpenTTD dynamically and move the needed libs into the bundle
# this way it's possible to move the app around without thinking about libs on the computer it runs on
# dynamic linking is recommended for several reasons, specially because static linking makes issues with OpenTTD
# to setup the lib paths, a few flags needs to be set (default setup should work if the libs were installed with fink)
#
# OSX_LIB_PATH: this is the path to where the lib files are located on the compiling computer. (default /sw/lib/)
# OSX_LOCAL_LIB_PATH: is the path that the binary will use to search for the libs.
#	it's always an absolute path except when the keyword @executable_path is used, then it's the binary
#	it's important to remember that it's the binary and not the bundle, so to get the bundle path, use @executable_path/../../../


ifdef RELEASE
	CONVERT_LIB_PATH:=convert_OSX_lib_path
endif

ifdef OSX_CHANGE_LIB_PATH
	CONVERT_LIB_PATH:=convert_OSX_lib_path
endif

ifndef OSX_LIB_PATH
	OSX_LIB_PATH:=/sw/lib/
endif

ifndef OSX_LOCAL_LIB_PATH
	OSX_LOCAL_LIB_PATH:=@executable_path/../lib/
endif

ifdef MAKEMOVEABLE
	CONVERT_LIB_PATH:=convert_OSX_lib_path
	OSX_LOCAL_LIB_PATH:=@executable_path/../lib/
	MOVE_LIB_INTO_BUNDLE:=move_lib_into_bundle
endif

# the rest is the targets. If more than one is called, they need to be called in the order they are written

# build the bundle. OSX wants to keep apps in bundles, so we will give it one
# the good thing about bundles is that you can keep extra files in them, so we keep lng files and a data dir in it
build_OSX_bundle:
	$(Q)rm -fr "$(OSXAPP)"
	$(Q)mkdir -p "$(OSXAPP)"/Contents/MacOS
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Resources
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Data
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Lang
	$(Q)echo "APPL????" > "$(OSXAPP)"/Contents/PkgInfo
	$(Q)cp os/macosx/openttd.icns "$(OSXAPP)"/Contents/Resources/openttd.icns
	$(Q)os/macosx/plistgen.sh "$(OSXAPP)" "$(REV)"
	$(Q)cp data/* "$(OSXAPP)"/Contents/Data/
	$(Q)cp lang/*.lng "$(OSXAPP)"/Contents/Lang/
	$(Q)cp $(TTD) "$(OSXAPP)"/Contents/MacOS/$(TTD)


# set the path that OpenTTD will use to search for dynamic libraries
$(CONVERT_LIB_PATH):
	$(Q)install_name_tool -change $(OSX_LIB_PATH)libSDL-1.2.0.dylib $(OSX_LOCAL_LIB_PATH)libSDL-1.2.0.dylib "$(OSXAPP)"/Contents/MacOS/$(TTD)
	$(Q)install_name_tool -change $(OSX_LIB_PATH)libpng12.0.dylib $(OSX_LOCAL_LIB_PATH)libpng12.0.dylib "$(OSXAPP)"/Contents/MacOS/$(TTD)


# copies the lib files into the bundle. This way you can move the bundle around like if it were statically linked
$(MOVE_LIB_INTO_BUNDLE):
	$(Q)mkdir -p "$(OSXAPP)"/Contents/lib/
	$(Q)cp -RH $(OSX_LIB_PATH)libSDL-1.2.0.dylib "$(OSXAPP)"/Contents/lib/libSDL-1.2.0.dylib
	$(Q)cp -RH $(OSX_LIB_PATH)libpng12.0.dylib "$(OSXAPP)"/Contents/lib/libpng12.0.dylib


# make the release disk image. Should only be used with releases and is a good and fast way to make sure to remember all the needed files
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
	$(Q)cp os/macosx/*.webloc "OpenTTD $(RELEASE)"
	$(Q)cp known-bugs.txt "OpenTTD $(RELEASE)"/known-bugs.txt
	$(Q)cp scenario/* "OpenTTD $(RELEASE)"/scenario/
	$(Q)/usr/bin/hdiutil create -ov -format UDZO -srcfolder "OpenTTD $(RELEASE)" openttd-"$(RELEASE)"-osx.dmg
	$(Q)rm -fr "OpenTTD $(RELEASE)"

$(OSX): $(TTD) build_OSX_bundle $(CONVERT_LIB_PATH)

.PHONY: release $(CONVERT_LIB_PATH) $(MOVE_LIB_INTO_BUNDLE) build_OSX_bundle
