# $Id: Makefile 3214 2005-11-17 19:43:37Z bjarni $
# This makefile is not a standalone makefile, but is called from the general one
# it contains targets specific to MacOS X

ifdef FAT_BINARY
FAT_BINARY:=build_universal_binary
endif

ifdef UNIVERSAL_x86_PART
# the bundle is build by the PPC compile when making universal binaries
BUILD_OSX_BUNDLE:=
else
BUILD_OSX_BUNDLE:=build_OSX_bundle
endif

# building an universal binary
# since we can only compile for PPC or x86 at any one time, we compile one and then
# we make clean and compile the other one. In the end we use lipo to join them together
# when this is done, we can continue with the targets from the first run, which is build_OSX_bundle

$(FAT_BINARY): $(TTD)
	$(Q)mkdir -p temp_binary_dir
	$(Q)cp $(TTD) temp_binary_dir/$(TTD)_a
	$(Q)rm -rf $(TTD) $(OBJS) # delete all .o files so we can compile for a new endian
	$(Q)make UNIVERSAL_x86_PART:=1
	$(Q)cp $(TTD) temp_binary_dir/$(TTD)_b
	@echo '===> Joining the PPC and x86 binaries into one universal one'
	$(Q)lipo temp_binary_dir/$(TTD)_a temp_binary_dir/$(TTD)_b -create -output $(TTD)
	$(Q)rm -rf temp_binary_dir

ifdef JAGUAR
JAGUAR_POSTFIX := -jaguar
endif

# build the bundle. OSX wants to keep apps in bundles, so we will give it one
# the good thing about bundles is that you can keep extra files in them, so we keep lng files and a data dir in it

$(BUILD_OSX_BUNDLE): $(TTD) $(FAT_BINARY)
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
	$(Q)cp docs/OSX_why_multiple_applications.txt "OpenTTD $(REV)/why a special Jaguar download.txt"
	$(Q)cp readme.txt "OpenTTD $(REV)/docs/"
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

.PHONY: release $(BUILD_OSX_BUNDLE) $(FAT_BINARY)
