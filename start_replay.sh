#!/bin/bash

WORKLOAD="$1"

#TODO get this time from the trace
INIT_TIME="$2"

CLOCK_DURATION="$3"

SLURM_REPLAY="/home/maximem/dev/github/slurm_simulator/slurm-replay"
SLURM_REPLAY_LIB="libwtime.so"

export PATH="$SLURM_REPLAY/submitter:$PATH"
export LD_LIBRARY_PATH="$SLURM_REPLAY/distime"

rm -Rf /dev/shm/*

# Add initial time
ticker -s $INIT_TIME

echo "Start submitter"
# Initiate job submitter
submitter -w $WORKLOAD -t template.script

# Initiate slurm
./start_slurm.sh $SLURM_REPLAY_LIB

END_TIME="$(( $INIT_TIME + $CLOCK_DURATION ))"
echo "Start clock for ${CLOCK_DURATION}s - end time is $END_TIME"
ticker -c "$END_TIME,0.1,5"

killall -q -9 slurmd slurmctld slurmstepd slurmdbd srun submitter ticker job_runner
