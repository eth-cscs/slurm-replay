
DISTIME_PATH ?= ../distime
SLURM_PATH ?= ../../slurm_newsim

# compiler flags
CC := gcc
CCFLAGS := -O0 -g
LDFLAGS :=
LIBRARIES := -L$(DISTIME_PATH) -lshmemclock -L$(SLURM_PATH)/lib -lslurm -lrt
MYSQL_LIBS := -L/usr/lib  -lmysqlclient -lpthread -lz -lm -ldl -lssl -lcrypto
INCLUDES := -I$(DISTIME_PATH) -I$(SLURM_PATH)/include

# Target rules
all: clean build

build: submitter starter job_runner trace_builder_mysql trace_builder_random trace_edit trace_list

submitter.o: submitter.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

submitter: submitter.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

starter.o: starter.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

starter: starter.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

job_runner.o: job_runner.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

job_runner: job_runner.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

trace_builder_mysql.o: trace_builder_mysql.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_builder_mysql: trace_builder_mysql.o
	$(CC) $(LDFLAGS) $(LIBRARIES) $(MYSQL_LIBS) -o $@ $+

trace_builder_random.o: trace_builder_random.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_builder_random: trace_builder_random.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

trace_edit.o: trace_edit.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_edit: trace_edit.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

trace_list.o: trace_list.c
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

trace_list: trace_list.o
	$(CC) $(LDFLAGS) $(LIBRARIES) -o $@ $+

clean:
	rm -f submitter submitter.o
	rm -f starter starter.o
	rm -f job_runner job_runner.o
	rm -f trace_builder_mysql trace_builder_mysql.o
	rm -f trace_builder_random trace_builder_random.o
	rm -f trace_edit trace_edit.o
	rm -f trace_list trace_list.o
