

# compiler flags
CC := gcc
CXX := g++
CCFLAGS := -O0 -g -fPIC
LDFLAGS := -shared
LIBRARIES := -lstdc++
INCLUDES := -I.

# Target rules
all: build

build: libshmem_pq.so

shmem_pq.o: shmem_pq.cpp
	$(CXX) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

time_pq.o: time_pq.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

libshmem_pq.so: shmem_pq.o time_pq.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

clean:
	rm -f libshmem_pq.so shmem_pq.o time_pq.o
