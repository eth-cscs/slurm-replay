
DISTIME_PATH ?= ../distime
SLURM_PATH ?= ../../slurm_newsim

# compiler flags
CC := gcc
#CCFLAGS := -O0 -g
CCFLAGS := -O2
LDFLAGS :=
LIB_SHMEMCLOCK := -L$(DISTIME_PATH) -lshmemclock -lrt
LIB_SLURM := -L$(SLURM_PATH)/lib -lslurm
LIB_MATH := -lm
MYSQL_LIBS := -L/usr/lib -L/usr/lib64/mysql  -lmysqlclient -lpthread -lz -lm -ldl -lssl -lcrypto
INCLUDES := -I$(DISTIME_PATH) -I$(SLURM_PATH)/include

# Target rules
all: clean build

build: submitter ticker job_runner trace_builder_mysql trace_list trace_compare node_controller

submitter.o: submitter.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

submitter: submitter.o
	$(CC) $(LDFLAGS) $(LIB_SHMEMCLOCK) $(LIB_SLURM) -o $@ $+

node_controller.o: node_controller.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

node_controller: node_controller.o
	$(CC) $(LDFLAGS) $(LIB_SHMEMCLOCK) $(LIB_SLURM) -o $@ $+


ticker.o: ticker.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

ticker: ticker.o
	$(CC) $(LDFLAGS) $(LIB_SHMEMCLOCK) -o $@ $+

job_runner.o: job_runner.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

job_runner: job_runner.o
	$(CC) $(LDFLAGS) $(LIB_SHMEMCLOCK) $(LIB_SLURM) -o $@ $+

trace_builder_mysql.o: trace_builder_mysql.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_builder_mysql: trace_builder_mysql.o
	$(CC) $(LDFLAGS) $(MYSQL_LIBS) -o $@ $+

trace_compare.o: trace_compare.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_compare: trace_compare.o
	$(CC) $(LDFLAGS) $(LIB_MATH) -o $@ $+

trace_list.o: trace_list.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_list: trace_list.o
	$(CC) $(LDFLAGS) -o $@ $+

clean:
	rm -f submitter submitter.o
	rm -f node_controller node_controller.o
	rm -f ticker ticker.o
	rm -f job_runner job_runner.o
	rm -f trace_builder_mysql trace_builder_mysql.o
	rm -f trace_compare trace_compare.o
	rm -f trace_list trace_list.o
