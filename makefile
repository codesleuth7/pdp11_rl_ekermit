#Makefile for embedded Kermit.
#
# Copyright (C) 1995, 2011,
#  Trustees of Columbia University in the City of New York.
#  All Rights Reserved.  See kermit.c for license.
# aout2lda --aout rlutil --lda rlutil.ptap --data-align 2 --text 0x400 --vector0
CC=pdp11-aout-gcc
CC2=pdp11-aout-gcc
#AR=pdp11-aout-ar
AS=pdp11-aout-as
#LD=pdp11-aout-ld
CFLAGS= -nostdlib -Ttext 0x400 -m45 -Xlinker -Map=output.map -Os -N -e _start -DNO_LP -DNODEBUG


OBJS= pdpmain.o kermit.o pdp11io.o rl.o console.o crt0.o
EK = pdp11
ALL = $(EK)

all: $(ALL)

ek: $(OBJS)
	$(CC) $(CFLAGS) -o ek $(OBJS)

ek.ptap: ek
	aout2lda --aout ek --lda ek.ptap --data-align 2 --text 0x400 --vector0

#Dependencies

pdpmain.o: pdpmain.c cdefs.h debug.h kermit.h platform.h makefile

kermit.o: kermit.c cdefs.h debug.h kermit.h makefile

unixio.o: unixio.c cdefs.h debug.h platform.h kermit.h makefile

rl.o: rl.c rl.h console.h makefile
	pdp11-aout-gcc -m45 -Os -c -o rl.o rl.c

crt0.o: crt0.s makefile

console.o: console.c console.h makefile

#Targets

crt0.o:
	$(CC) $(CFLAGS) -c -o crt0.o crt0.s

#Build with cc.
cc:
	make ek

#	@UNAME=`uname` ; make "CC=pdp11-aout-gcc" "CC2=pdp11-aout-gcc" "CFLAGS= -nostdlib -Ttext 0x400 -m45 -Xlinker -Map=output.map -Os -N -e _start -DMINSIZE -DOBUFLEN=256 -DNODEBUG" ek ; make ek.ptap

pdp11:
	@UNAME=`uname` ; make "CC=pdp11-aout-gcc" "CC2=pdp11-aout-gcc" "CFLAGS= -nostdlib -Ttext 0x400 -m45 -Xlinker -Map=output.map -Os -N -e _start -DNO_LP -DNODEBUG" ek ; make ek.ptap
	./map2oct.pl < output.map > oct.map; mv -v oct.map output.map

#Build with gcc.
gcc:
	@UNAME=`uname` ; make "CC=gcc" "CC2=gcc" "CFLAGS=-D$$UNAME -O2" ek

#Ditto but no debugging.
gccnd:
	make "CC=gcc" "CC2=gcc" "CFLAGS=-DNODEBUG -O2" ek

#Build with gcc, Receive-Only, minimum size and features.
gccmin:
	make "CC=gcc" "CC2=gcc" \
	"CFLAGS=-DMINSIZE -DOBUFLEN=256 -DFN_MAX=16 -O2" ek

#Ditto but Receive-Only:
gccminro:
	make "CC=gcc" "CC2=gcc" \
	"CFLAGS=-DMINSIZE -DOBUFLEN=256 -DFN_MAX=16 -DRECVONLY -O2" ek

#Minimum size, receive-only, but with debugging:
gccminrod:
	make "CC=gcc" "CC2=gcc" \
	"CFLAGS=-DMINSIZE -DOBUFLEN=256 -DFN_MAX=16 -DRECVONLY -DDEBUG -O2" ek

#HP-UX 9.0 or higher with ANSI C.
hp:
	make "SHELL=/usr/bin/sh" CC=/opt/ansic/bin/cc CC2=/opt/ansic/bin/cc \
	ek "CFLAGS=-DHPUX -Aa"

#To get profile, build this target, run it, then "gprof ./ek > file".
gprof:
	make "CC=gcc" "CC2=gcc" ek "CFLAGS=-DNODEBUG -pg" "LNKFLAGS=-pg"

clean:
	rm -f $(OBJS) core

makewhat:
	@echo 'Defaulting to gcc...'
	make gcc

#End of Makefile
