

# compiler flags
CC := gcc
CXX := g++
AR:=ar
ARFLAGS=rcs
RANLIB=ranlib
#CCFLAGS := -O0 -g -fPIC
CCFLAGS := -O2 -fPIC
LDFLAGS := -shared
LIBRARIES :=
INCLUDES := -I.

# Target rules
all: clean build

build: libshmemclock.a libwtime.so

shmemclock.o: shmemclock.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

wrapper.o: wrapper.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

libshmemclock.a: shmemclock.o
	$(AR) $(ARFLAGS) $@ $+
	$(RANLIB) $@

libwtime.so: wrapper.o
	$(CC) $(LDFLAGS) -L. -lshmemclock -lrt -o $@ $+

clean:
	rm -f libshmemclock.a libwtime.so shmemclock.o wrapper.o
