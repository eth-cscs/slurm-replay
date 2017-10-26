#!/bin/bash

WORKLOAD=$1

#TODO get this time from the trace
INIT_TIME=$2

SLURM_DIR="/home/maximem/dev/github/slurm_simulator/slurm_newsim"
SLURM_REPLAY="/home/maximem/dev/github/slurm_simulator/slurm-replay"

killall -9 slurmd slurmctld submitter

export PATH=$SLURM_DIR/bin:$SLURM_DIR/sbin:$SLURM_REPLAY/submitter:$PATH
export LD_LIBRARY_PATH=$SLURM_DIR/lib:$SLURM_REPLAY/distime

rm -Rf /dev/shm/*
rm -Rf /tmp/slurm-*.out
rm -Rf $SLURM_DIR/tmp/slurmd/*
rm -Rf $SLURM_DIR/tmp/state/*
rm -Rf $SLURM_DIR/tmp/job*
rm -f $SLURM_DIR/tmp/storage.txt
rm -f $SLURM_DIR/tmp/slurmctld.*

# Add initial time
starter -t $2 -a "A" -s "Insert first time clock: $2"

echo "Start submitter"
# Initiate job submitter and populate submission time from trace
#echo "launch this - 10 seconds"
submitter -w $1 -t template.script
#sleep 10

echo "Start slurm"
# Initiate slurm
for k in $(seq 12 1 18); do
    slurmd -N nid000$k
done
sleep 5
$3 slurmctld

sleep 5
# Make the clock progress from one discretized time tick
# submitter and slurm daemons will be able to make time progress
#echo "Release clock"
#./starter -t $2 -a "R" -s "Initiate the replay at time $2"

LEN_CLOCK=60
END_TIME=$(( $2 + $LEN_CLOCK ))
echo "Start clock for ${LEN_CLOCK}s - end time is $END_TIME"
starter -a 'C' -t $END_TIME

# Wait indefinitely until the user stop the replay
#sleep infinity
