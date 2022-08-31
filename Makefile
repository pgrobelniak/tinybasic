all: basic
WARN=
#WARN=-Wall -Wno-unused-parameter -Wno-parentheses -Wno-unused-result
DEPS=`sdl2-config --libs --cflags` -lm -DSDL
basic: basic.c basic.h hardware-posix.h sdl.h scancodes.h vt52rom.h
	cc $(WARN) -O3 -o basic basic.c $(DEPS)
