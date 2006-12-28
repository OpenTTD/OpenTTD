# $Id: Makefile 3214 2005-11-17 19:43:37Z bjarni $
# This makefile is not a standalone makefile, but is called from the general one
# it contains targets specific to MacOS X

ifdef MACOSX_BUILD

DEPS_MAGIC := $(shell mkdir -p $(sort $(dir $(OBJS:%.o=.OSX/%))))

ifdef OTTD_PPC
OTTD_PPC    :=.OSX/openttd.ppc
OBJS_ppc    := $(OBJS:%.o=.OSX/%.o.ppc)
ifndef CC_PPC
CC_PPC      := $(CC) -arch ppc
endif
ifndef CXX_PPC
CXX_PPC     := $(CXX) -arch ppc
endif
endif

ifdef OTTD_i386
OTTD_i386   :=.OSX/openttd.i386
OBJS_i386   := $(OBJS:%.o=.OSX/%.o.i386)
ifndef CC_I386
CC_I386     := $(CC) -arch i386
endif
ifndef CXX_I386
CXX_I386    := $(CXX) -arch i386
endif
endif

ifdef OTTD_PPC970
OTTD_PPC970 :=.OSX/openttd.ppc970
OBJS_ppc970 := $(OBJS:%.o=.OSX/%.o.ppc970)
ifndef CC_PPC970
CC_PPC970   := $(CC) -arch ppc970
endif
ifndef CXX_PPC970
CXX_PPC970  := $(CXX) -arch ppc970
endif
endif

ifndef LIPO
LIPO := lipo
endif
ifndef STRIP
STRIP := strip
endif

# targets to link OpenTTD
$(TTD): $(OTTD_PPC) $(OTTD_i386) $(OTTD_PPC970)
	@echo '===> Linking $@ into a single file'
	$(Q)$(LIPO) -create -output $@ $(OTTD_PPC) $(OTTD_i386) $(OTTD_PPC970)

$(OTTD_PPC): $(MAKE_CONFIG) $(OBJS_ppc)
	@echo '[PowerPC] Linking $(TTD)'
	$(Q)$(CXX_PPC) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS_ppc) $(LIBS) $(LDFLAGS_PPC) -o $@

$(OTTD_i386): $(MAKE_CONFIG) $(OBJS_i386)
	@echo '[i386] Linking $(TTD)'
	$(Q)$(CXX_I386) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS_i386) $(LIBS) $(LDFLAGS_i386) -o $@

$(OTTD_PPC970): $(MAKE_CONFIG) $(OBJS_ppc970)
	@echo '[PowerPC G5] Linking $(TTD)'
	$(Q)$(CXX_PPC970) $(LDFLAGS) $(TTDLDFLAGS) $(OBJS_ppc970) $(LIBS) $(LDFLAGS_PPC) $(G5_FLAGS) -o $@

# targets to compile each c, m and cpp file
.OSX/%.o.ppc: %.c      .deps/%.d
	@echo '[PowerPC] Compiling $<'
	$(Q)$(CC_PPC) $(CC_CFLAGS) $(CFLAGS) $(CFLAGS_PPC) $(CDEFS) -c -o $@ $<

.OSX/%.o.ppc: %.cpp    .deps/%.d
	@echo '[PowerPC] Compiling $<'
	$(Q)$(CXX_PPC) $(CFLAGS) $(CFLAGS_PPC) $(CDEFS) -c -o $@ $<

.OSX/%.o.ppc: %.m      .deps/%.d
	@echo '[PowerPC] Compiling $<'
	$(Q)$(CC_PPC) $(CC_CFLAGS) $(CFLAGS) $(CFLAGS_PPC) $(CDEFS) -c -o $@ $<

.OSX/%.o.i386: %.c     .deps/%.d
	@echo '[i386] Compiling $<'
	$(Q)$(CC_I386) $(CC_CFLAGS) $(CFLAGS) $(CFLAGS_i386) $(CDEFS) -c -o $@ $<

.OSX/%.o.i386: %.cpp   .deps/%.d
	@echo '[i386] Compiling $<'
	$(Q)$(CXX_I386) $(CFLAGS) $(CFLAGS_i386) $(CDEFS) -c -o $@ $<

.OSX/%.o.i386: %.m     .deps/%.d
	@echo '[i386] Compiling $<'
	$(Q)$(CC_I386) $(CC_CFLAGS) $(CFLAGS) $(CFLAGS_i386) $(CDEFS) -c -o $@ $<

.OSX/%.o.ppc970: %.c   .deps/%.d
	@echo '[PowerPC G5] Compiling $<'
	$(Q)$(CC_PPC970) $(CC_CFLAGS) $(CFLAGS) $(CFLAGS_PPC) $(G5_FLAGS) $(CDEFS) -c -o $@ $<

.OSX/%.o.ppc970: %.cpp .deps/%.d
	@echo '[PowerPC G5] Compiling $<'
	$(Q)$(CXX_PPC970) $(CFLAGS) $(CFLAGS_PPC) $(G5_FLAGS) $(CDEFS) -c -o $@ $<

.OSX/%.o.ppc970: %.m   .deps/%.d
	@echo '[PowerPC G5] Compiling $<'
	$(Q)$(CC_PPC970) $(CC_CFLAGS) $(CFLAGS) $(CFLAGS_PPC) $(G5_FLAGS) $(CDEFS) -c -o $@ $<

endif

# manual strip, as the -s option fails
$(OSX_STRIP): $(TTD)
	$(Q)$(STRIP) openttd


# build the bundle. OSX wants to keep apps in bundles, so we will give it one
# the good thing about bundles is that you can keep extra files in them, so we keep lng files and a data dir in it

BUILD_OSX_BUNDLE: $(TTD) $(OSX_STRIP)
	@echo '===> Building application bundle'
	$(Q)rm -fr "$(OSXAPP)"
	$(Q)mkdir -p "$(OSXAPP)"/Contents/MacOS
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Resources
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Data
	$(Q)mkdir -p "$(OSXAPP)"/Contents/Lang
	$(Q)echo "APPL????" > "$(OSXAPP)"/Contents/PkgInfo
	$(Q)cp os/macosx/openttd.icns "$(OSXAPP)"/Contents/Resources/openttd.icns
	$(Q)os/macosx/plistgen.sh "$(OSXAPP)" "$(REV)"
	$(Q)cp -R data/* "$(OSXAPP)"/Contents/Data/
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
	$(Q)cp docs/console.txt "OpenTTD $(REV)/docs/"
	$(Q)cp COPYING "OpenTTD $(REV)/docs/"
	$(Q)cp changelog.txt "OpenTTD $(REV)/docs/"
	$(Q)cp os/macosx/*.webloc "OpenTTD $(REV)"
	$(Q)cp known-bugs.txt "OpenTTD $(REV)/known-bugs.txt"
	$(Q)cp -R scenario/* "OpenTTD $(REV)/scenario/"
	$(Q)hdiutil create -ov -format UDZO -srcfolder "OpenTTD $(REV)" openttd-"$(REV)"-osx.dmg
	$(Q)rm -fr "OpenTTD $(REV)"

$(OSX): $(TTD) $(OSX_STRIP) BUILD_OSX_BUNDLE

.PHONY: release BUILD_OSX_BUNDLE
