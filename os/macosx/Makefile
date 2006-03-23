# $Id: Makefile 3214 2005-11-17 19:43:37Z bjarni $
# This makefile is not a standalone makefile, but is called from the general one
# it contains targets specific to MacOS X

ifdef TRIPLE_BINARY
# this is to add G5_FLAGS to ppc970 builds only. If the ability to add flags to a single arch only shows up in the future
# we can get rid of this. Xcode supports arch dependant flags, but we can't do it in the makefile (yet?)
%.o: %.c $(MAKE_CONFIG)
	@echo '===> Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CDEFS) -arch ppc -c -arch i386 -o $@.uni $<
	$(Q)$(CC) $(CFLAGS) $(CDEFS) $(G5_FLAGS) -arch ppc970 -c -o $@.ppc970 $<
	$(Q)lipo -create -output $@ $@.uni $@.ppc970
	$(Q)rm $@.uni $@.ppc970

%.o: %.cpp  $(MAKE_CONFIG)
	@echo '===> Compiling $<'
	$(Q)$(CXX) $(CFLAGS) $(CDEFS) -arch ppc -arch i386 -c -o $@.uni $<
	$(Q)$(CXX) $(CFLAGS) $(CDEFS) $(G5_FLAGS) -arch ppc970 -c -o $@.ppc970 $<
	$(Q)lipo -create -output $@ $@.uni $@.ppc970
	$(Q)rm $@.uni $@.ppc970

%.o: %.m  $(MAKE_CONFIG)
	@echo '===> Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CDEFS) -arch ppc -arch i386 -c -o $@.uni $<
	$(Q)$(CC) $(CFLAGS) $(CDEFS) -arch ppc970 $(G5_FLAGS) -c -o $@.ppc970 $<
	$(Q)lipo -create -output $@ $@.uni $@.ppc970
	$(Q)rm $@.uni $@.ppc970
endif

ifdef UNIVERSAL_PPC_PART
# the bundle is build by the PPC compile when making universal binaries
BUILD_OSX_BUNDLE:=
else
BUILD_OSX_BUNDLE:=build_OSX_bundle
endif

ifdef JAGUAR
JAGUAR_POSTFIX := -jaguar
endif

# build the bundle. OSX wants to keep apps in bundles, so we will give it one
# the good thing about bundles is that you can keep extra files in them, so we keep lng files and a data dir in it

$(BUILD_OSX_BUNDLE): $(TTD)
	@echo '===> Building application bundle'
	$(Q)rm -fr "$(OSXAPP)"
	$(Q)mkdir -p "$(OSXAPP)"/Contents/MacOS
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Resources
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Data
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Lang
	$(Q)echo "APPL????" > "$(OSXAPP)"/Contents/PkgInfo
	$(Q)cp os/macosx/openttd.icns "$(OSXAPP)"/Contents/Resources/openttd.icns
	$(Q)os/macosx/plistgen.sh "$(OSXAPP)" "$(REV)"
	$(Q)cp data/* "$(OSXAPP)"/Contents/Data/
	$(Q)cp os/macosx/splash.png "$(OSXAPP)"/Contents/Data/
	$(Q)cp lang/*.lng "$(OSXAPP)"/Contents/Lang/
	$(Q)cp $(TTD) "$(OSXAPP)"/Contents/MacOS/$(TTD)

# make the release disk image. Should only be used with releases and is a good and fast way to make sure to remember all the needed files
release: all
	@echo '===> Building release disk image'
	$(Q)mkdir -p "OpenTTD $(REV)"
	$(Q)mkdir -p "OpenTTD $(REV)/docs"
	$(Q)mkdir -p "OpenTTD $(REV)/scenario"
	$(Q)cp -R $(OSXAPP) "OpenTTD $(REV)/"
	$(Q)cp docs/OSX_install_instructions.txt "OpenTTD $(REV)/How to install (please read).txt"
	$(Q)cp readme.txt "OpenTTD $(REV)/docs/"
	$(Q)cp docs/README_if_game_crashed_on_OSX.txt "OpenTTD $(REV)/docs/readme if crashed on OSX.txt"
	$(Q)cp docs/console.txt "OpenTTD $(REV)/docs/"
	$(Q)cp COPYING "OpenTTD $(REV)/docs/"
	$(Q)cp changelog.txt "OpenTTD $(REV)/docs/"
	$(Q)cp docs/README_if_game_crashed_on_OSX.txt "OpenTTD $(REV)/docs/"
	$(Q)cp os/macosx/*.webloc "OpenTTD $(REV)"
	$(Q)cp known-bugs.txt "OpenTTD $(REV)/known-bugs.txt"
	$(Q)cp scenario/* "OpenTTD $(REV)/scenario/"
	$(Q)/usr/bin/hdiutil create -ov -format UDZO -srcfolder "OpenTTD $(REV)" openttd-"$(REV)"-osx"$(JAGUAR_POSTFIX)".dmg
	$(Q)rm -fr "OpenTTD $(REV)"

$(OSX): $(TTD) $(BUILD_OSX_BUNDLE)

.PHONY: release $(BUILD_OSX_BUNDLE) $(UNIVERSAL_BINARY)
