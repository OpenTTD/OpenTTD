# $Id: Makefile 3214 2005-11-17 19:43:37Z bjarni $
# This makefile is not a standalone makefile, but is called from the general one
# it contains targets specific to MacOS X

ifdef BUILD_UNIVERSAL_BINARY
BUILD_UNIVERSAL_BINARY:=build_universal_binary
endif

# setting the default vars for making universal binaries
# this can be overwritten in Makefile.config
# default values are for PPC host and commends are for x86 hosts (so you know what to put in)
# the absolute path is the one Apple uses in Xcode 2.1
ifndef CC_UNI
	# powerpc-apple-darwin8-gcc-4.0.0
	CC_UNI:=i686-apple-darwin8-gcc-4.0.0
endif

ifndef CFLAGS_UNI
	# -arch ppc
	CFLAGS_UNI:= -isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch i386
endif

ifndef LDFLAGS_UNI
	LDFLAGS_UNI:= -Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk
endif

ifdef UNIVERSAL_OTHER_HALF
# if UNIVERSAL_OTHER_HALF is defined, we know that the original makefile call will build the bundle
BUILD_OSX_BUNDLE:=
else
BUILD_OSX_BUNDLE:=build_OSX_bundle
endif

# building an universal binary
# since we can only compile for PPC or x86 at any one time, we compile one and then
# we make clean and compile the other one. In the end we use lipo to join them together
# when this is done, we can continue with the targets from the first run, which is build_OSX_bundle

$(BUILD_UNIVERSAL_BINARY): $(TTD)
	$(Q)mkdir -p temp_binary_dir
	$(Q)cp $(TTD) temp_binary_dir/$(TTD)_a
	@echo '===> Cleaning up to build for the other architecture'
	$(Q)make clean
	$(Q)make UNIVERSAL_OTHER_HALF:=1
	$(Q)cp $(TTD) temp_binary_dir/$(TTD)_b
	@echo '===> Joining binaries into one universal one'
	$(Q)lipo temp_binary_dir/$(TTD)_a temp_binary_dir/$(TTD)_b -create -output $(TTD)
	$(Q)rm -rf temp_binary_dir



# build the bundle. OSX wants to keep apps in bundles, so we will give it one
# the good thing about bundles is that you can keep extra files in them, so we keep lng files and a data dir in it

$(BUILD_OSX_BUNDLE): $(TTD) $(BUILD_UNIVERSAL_BINARY)
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

$(OSX): $(TTD) $(BUILD_OSX_BUNDLE)

.PHONY: release $(BUILD_OSX_BUNDLE) $(BUILD_UNIVERSAL_BINARY)
