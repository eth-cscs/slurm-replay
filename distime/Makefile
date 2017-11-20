

# compiler flags
CC := gcc
CXX := g++
#CCFLAGS := -O0 -g -fPIC
CCFLAGS := -O2 -fPIC
LDFLAGS := -shared
LIBRARIES :=
INCLUDES := -I.

# Target rules
all: clean build

build: libshmemclock.so libwtime.so

shmemclock.o: shmemclock.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

wrapper.o: wrapper.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

libshmemclock.so: shmemclock.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

libwtime.so: wrapper.o
	$(CC) $(LDFLAGS) -L. -lshmemclock -lrt -o $@ $+

clean:
	rm -f libshmemclock.so libwtime.so shmemclock.o wrapper.o
