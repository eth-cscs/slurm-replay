#!/bin/bash

WORKLOAD="$1"

#TODO get this time from the trace
INIT_TIME="$2"

CLOCK_DURATION="$3"

SLURM_DIR="/home/maximem/dev/github/slurm_simulator/slurm_newsim"
SLURM_REPLAY="/home/maximem/dev/github/slurm_simulator/slurm-replay"
SLURM_REPLAY_LIB="libwtime.so"

export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$SLURM_REPLAY/submitter:$PATH"
export LD_LIBRARY_PATH="$SLURM_DIR/lib:$SLURM_REPLAY/distime"

rm -Rf /dev/shm/*
rm -Rf /tmp/slurm-*.out
rm -Rf $SLURM_DIR/tmp/*
mkdir $SLURM_DIR/tmp/state
mkdir $SLURM_DIR/tmp/slurmd

killall -9 slurmd slurmctld slurmstepd srun submitter starter job_runner

# Add initial time
starter -t $INIT_TIME -a "S"

echo "Start submitter"
# Initiate job submitter
submitter -w $WORKLOAD -t template.script

echo "Start slurm"
# Initiate slurm
for k in $(seq 12 1 18); do
    LD_PRELOAD="$SLURM_REPLAY_LIB" slurmd -N nid000$k
done
sleep 5
LD_PRELOAD="$SLURM_REPLAY_LIB" slurmctld
sleep 5

END_TIME="$(( $INIT_TIME + $CLOCK_DURATION ))"
echo "Start clock for ${CLOCK_DURATION}s - end time is $END_TIME"
starter -a 'C' -t "$END_TIME"

#killall -9 slurmd slurmctld slurmstepd srun submitter starter job_runner
