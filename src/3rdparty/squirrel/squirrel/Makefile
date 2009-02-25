SQUIRREL= ..


OUT= $(SQUIRREL)/lib/libsquirrel.a
INCZ= -I$(SQUIRREL)/include -I. -Iinclude
DEFS=
LIB=

OBJS= \
	sqapi.o \
	sqbaselib.o \
	sqcompiler.o \
	sqdebug.o \
	sqlexer.o \
	sqobject.o \
	sqparser.o \
	sqstate.o \
	sqtable.o \
	sqvm.o \
	sqmem.o \
	sqclass.o

SRCS= \
	sqapi.cpp \
	sqbaselib.cpp \
	sqfuncstate.cpp \
	sqdebug.cpp \
	sqlexer.cpp \
	sqobject.cpp \
	sqcompiler.cpp \
	sqstate.cpp \
	sqtable.cpp \
	sqmem.cpp \
	sqvm.cpp \
	sqclass.cpp



sq32:
	gcc -O2  -fno-rtti -Wall -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o

sqprof:
	gcc -O2 -pg -fno-rtti -pie -gstabs -g3 -Wall -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o

sq64:
	gcc -O2  -D_SQ64 -fno-rtti -Wall -c $(SRCS) $(INCZ) $(DEFS)
	ar rc $(OUT) *.o

clean:
	rm -f $(OUT) $(SRCS:%.cpp=%.o)
