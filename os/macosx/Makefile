# $Id: Makefile 3214 2005-11-17 19:43:37Z bjarni $
# This makefile is not a standalone makefile, but is called from the general one
# it contains targets specific to MacOS X

ifdef NATIVE_OSX

DEPS_MAGIC := $(shell mkdir -p $(sort $(dir $(OBJS:%.o=.OSX/%))))

ifdef OTTD_PPC
OTTD_PPC    :=.OSX/openttd.ppc
OBJS_ppc    := $(OBJS:%.o=.OSX/%.o.ppc)
endif

ifdef OTTD_i386
OTTD_i386   :=.OSX/openttd.i386
OBJS_i386   := $(OBJS:%.o=.OSX/%.o.i386)
endif

ifdef OTTD_PPC970
OTTD_PPC970 :=.OSX/openttd.ppc970
OBJS_ppc970 := $(OBJS:%.o=.OSX/%.o.ppc970)
endif

# targets to link OpenTTD
$(TTD): $(OTTD_PPC) $(OTTD_i386) $(OTTD_PPC970)
	@echo '===> Linking $@ into a single file'
	$(Q)lipo -create -output $@ $(OTTD_PPC) $(OTTD_i386) $(OTTD_PPC970)

$(OTTD_PPC): $(MAKE_CONFIG) $(OBJS_ppc)
	@echo '[PowerPC] Linking $(TTD)'
	$(Q)$(CC) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS_ppc) $(LIBS) $(LDFLAGS_PPC) -arch ppc -o $@

$(OTTD_i386): $(MAKE_CONFIG) $(OBJS_i386)
	@echo '[i386] Linking $(TTD)'
	$(Q)$(CC) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS_i386) $(LIBS) $(LDFLAGS_i386) -arch i386 -o $@

$(OTTD_PPC970): $(MAKE_CONFIG) $(OBJS_ppc970)
	@echo '[PowerPC G5] Linking $(TTD)'
	$(Q)$(CC) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS_ppc970) $(LIBS) $(LDFLAGS_PPC) $(G5_FLAGS) -arch ppc970 -o $@

# targets to compile each c, m and cpp file
.OSX/%.o.ppc: %.c      .deps/%.d
	@echo '[PowerPC] Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_PPC) $(CDEFS) -arch ppc -c -o $@ $<

.OSX/%.o.ppc: %.cpp    .deps/%.d
	@echo '[PowerPC] Compiling $<'
	$(Q)$(CPP) $(CFLAGS) $(CFLAGS_PPC) $(CDEFS) -arch ppc -c -o $@ $<

.OSX/%.o.ppc: %.m      .deps/%.d
	@echo '[PowerPC] Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_PPC) $(CDEFS) -arch ppc -c -o $@ $<

.OSX/%.o.i386: %.c     .deps/%.d
	@echo '[i386] Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_i386) $(CDEFS) -arch i386 -c -o $@ $<

.OSX/%.o.i386: %.cpp   .deps/%.d
	@echo '[i386] Compiling $<'
	$(Q)$(CPP) $(CFLAGS) $(CFLAGS_i386) $(CDEFS) -arch i386 -c -o $@ $<

.OSX/%.o.i386: %.m     .deps/%.d
	@echo '[i386] Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_i386) $(CDEFS) -arch i386 -c -o $@ $<

.OSX/%.o.ppc970: %.c   .deps/%.d
	@echo '[PowerPC G5] Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_PPC) $(G5_FLAGS) $(CDEFS) -arch ppc970 -c -o $@ $<

.OSX/%.o.ppc970: %.cpp .deps/%.d
	@echo '[PowerPC G5] Compiling $<'
	$(Q)$(CPP) $(CFLAGS) $(CFLAGS_PPC) $(G5_FLAGS) $(CDEFS) -arch ppc970 -c -o $@ $<

.OSX/%.o.ppc970: %.m   .deps/%.d
	@echo '[PowerPC G5] Compiling $<'
	$(Q)$(CC) $(CFLAGS) $(CFLAGS_PPC) $(G5_FLAGS) $(CDEFS) -arch ppc970 -c -o $@ $<

endif

ifdef JAGUAR
JAGUAR_POSTFIX := -jaguar
endif

# build the bundle. OSX wants to keep apps in bundles, so we will give it one
# the good thing about bundles is that you can keep extra files in them, so we keep lng files and a data dir in it

BUILD_OSX_BUNDLE: $(TTD)
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
	$(Q)hdiutil create -ov -format UDZO -srcfolder "OpenTTD $(REV)" openttd-"$(REV)"-osx"$(JAGUAR_POSTFIX)".dmg
	$(Q)rm -fr "OpenTTD $(REV)"

$(OSX): $(TTD) BUILD_OSX_BUNDLE

.PHONY: release $(BUILD_OSX_BUNDLE) $(UNIVERSAL_BINARY)
