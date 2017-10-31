

# compiler flags
CC := gcc
CXX := g++
CCFLAGS := -O0 -g -fPIC
LDFLAGS := -shared
LIBRARIES := -lstdc++
INCLUDES := -I.

# Target rules
all: clean build

build: libshmemclock.so libwtime.so

#shmem_pq.o: shmem_pq.cpp
#	$(CXX) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

#shmem_pq.o: shmem_pq.c
#	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

shmemclock.o: shmemclock.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

#time_shmemclock.o: time_shmemclock.c
#	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

wrapper.o: wrapper.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

libshmemclock.so: shmemclock.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

libwtime.so: wrapper.o
	$(CC) $(LDFLAGS) -L. -lshmemclock -lrt -o $@ $+

clean:
	rm -f libshmemclock.so libwtime.so shmemclock.o wrapper.o
