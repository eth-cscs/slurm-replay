
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
INCLUDES := -I$(DISTIME_PATH) -I$(SLURM_PATH)/include -I../submitter

# Target rules
all: clean build

build: trace_builder_mysql trace_list trace_compare trace_metrics

trace_builder_mysql.o: trace_builder_mysql.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_builder_mysql: trace_builder_mysql.o
	$(CC) $(LDFLAGS) $(MYSQL_LIBS) $(LIB_COMMON) -o $@ $+

trace_compare.o: trace_compare.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_compare: trace_compare.o
	$(CC) $(LDFLAGS) $(LIB_MATH) -o $@ $+

trace_metrics.o: trace_metrics.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_metrics: trace_metrics.o
	$(CC) $(LDFLAGS) $(LIB_MATH) -o $@ $+

trace_list.o: trace_list.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_list: trace_list.o
	$(CC) $(LDFLAGS) -o $@ $+

clean:
	rm -f trace_builder_mysql trace_builder_mysql.o
	rm -f trace_compare trace_compare.o
	rm -f trace_metrics trace_metrics.o
	rm -f trace_list trace_list.o
