all : rev.c

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

