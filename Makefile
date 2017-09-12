#!/bin/sh

LIBPREFIX = lib
MODULE    = latm2adts

LIBSRC=../${MODULE}.c

CFLAGS +=  -std=c99

TARGETS = ${LIBPREFIX}${MODULE}.a

OBJS = ${MODULE}.o 

all:${TARGETS}


${TARGETS}: ${OBJS}
	$(AR) -rcs $(TARGETS) *.o

clean:
	rm -rf ${TARGETS}
	rm -rf ${OBJS}


