#
# a very simple Makefile for miniLZO
#
# Copyright (C) 1996-2002 Markus F.X.J. Oberhumer
#

SOURCES = testmini.c minilzo.c

default all:
	@echo "Please choose one of the following targets:"
	@echo "    gcc gcc-i386 gcc-rs6000"
	@echo "    visualc watcomc watcomc16"
	@echo "    hpux hpux9"


#
# gcc
#
gcc:
	gcc -Wall -O2 -s -o testmini $(SOURCES)

gcc-i386:
	gcc -Wall -O2 -fomit-frame-pointer -fno-strength-reduce -s -o testmini $(SOURCES)

gcc-rs6000:
	gcc -Wall -O2 -fno-schedule-insns -fno-schedule-insns2 -o testmini $(SOURCES)


#
# Windows
#
visualc:
	cl -nologo -ML -W3 -O2 -GF $(SOURCES)

watcomc:
	wcl386 -zq -mf -5r -zc -wx -oneatx $(SOURCES)

watcomc16:
	wcl -zq -ml -5 -zc -wx -oneatx $(SOURCES)


#
# UNIX
#

hpux:
	cc -Ae -o testmini $(SOURCES)

hpux9:
	cc -Aa -D_HPUX_SOURCE -o testmini $(SOURCES)


#
# other targets
#
clean:
	rm -f testmini testmini.exe *.err *.o *.obj core

