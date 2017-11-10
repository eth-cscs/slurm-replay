#!/bin/bash

WORKLOAD="$1"
if [ -z "$WORKLOAD" ]; then
    echo "Please provide a trace file"
    exit 1
fi


SLURM_DIR="/home/maximem/dev/github/slurm_simulator/slurm_newsim"
SLURM_REPLAY="/home/maximem/dev/github/slurm_simulator/slurm-replay"
SLURM_REPLAY_LIB="libwtime.so"

export PATH="$SLURM_REPLAY/submitter:$PATH"
export LD_LIBRARY_PATH="$SLURM_REPLAY/distime:$SLURM_DIR/lib:$LD_LIBRARY_PATH"

trap "killall -q -9 slurmd slurmctld slurmstepd slurmdbd srun submitter ticker job_runner" SIGINT SIGTERM EXIT

rm -Rf /dev/shm/*

TIME_PAD=60
START_TIME="$(trace_list -n -w $1 | awk '{print $5;}' | head -n 1)"
START_TIME="$(( $START_TIME - $TIME_PAD ))"
END_TIME="$(trace_list -n -w $1 | awk '{print $8;}' | tail -n 1)"
END_TIME="$(( $END_TIME + $TIME_PAD ))"


# Add initial time
ticker -s "$START_TIME"

echo -n "Start submitter... "
# Initiate job submitter
submitter -w "$WORKLOAD" -t template.script
echo "done."

# Initiate slurm and slurmdb
./start_slurm.sh "$SLURM_REPLAY/distime" "$SLURM_REPLAY_LIB"

CLOCK_DURATION="$(( $END_TIME - $START_TIME ))"
echo "Clock: $(date -d @$START_TIME "+%Y-%m-%d %H:%M:%S") --- $(date -d @$END_TIME "+%Y-%m-%d %H:%M:%S") --- ${CLOCK_DURATION}[s]"
ticker -c "$END_TIME,0.1,5"

