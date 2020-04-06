
DISTIME_PATH ?= ../distime
SLURM_PATH ?= ../../slurmR

# compiler flags
OS := $(shell uname)
CC := gcc
#CCFLAGS := -O0 -g
CCFLAGS := -O2
LDFLAGS :=
LIB_SHMEMCLOCK := -L$(DISTIME_PATH) -lshmemclock -lrt
LIB_SLURM := -L$(SLURM_PATH)/lib -lslurm
LIB_MATH := -lm
ifeq ($(OS),Darwin)
  MYSQL_LIBS :=  -L/usr/local/lib -lmysqlclient
  LIB_COMMON := -L/usr/local/opt/openssl/lib -L/usr/lib -lpthread -lz -lm -ldl -lssl -lcrypto
else
  MYSQL_LIBS := -L/usr/lib/x86_64-linux-gnu -lmariadb
  LIB_COMMON :=
endif
INCLUDES := -I$(DISTIME_PATH) -I$(SLURM_PATH)/include

# Target rules
all: clean build

build: submitter ticker job_runner node_controller db_correctness

submitter.o: submitter.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

submitter: submitter.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIB_SLURM) $(LIB_SHMEMCLOCK) $(LIB_MATH)

node_controller.o: node_controller.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

node_controller: node_controller.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIB_SHMEMCLOCK) $(LIB_SLURM)

ticker.o: ticker.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

ticker: ticker.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIB_SHMEMCLOCK) $(LIB_SLURM)

job_runner.o: job_runner.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

job_runner: job_runner.o
	$(CC) $(LDFLAGS) -o $@ $+ $(LIB_SHMEMCLOCK) $(LIB_SLURM) $(LIB_MATH)

db_correctness.o: db_correctness.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

db_correctness: db_correctness.o
	$(CC) $(LDFLAGS) $(MYSQL_LIBS) $(LIB_COMMON) -o $@ $+
clean:
	rm -f submitter submitter.o
	rm -f node_controller node_controller.o
	rm -f ticker ticker.o
	rm -f job_runner job_runner.o
	rm -f db_correctness db_correctness.o
