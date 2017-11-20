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
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_REPLAY/distime:$SLURM_DIR/lib:$LD_LIBRARY_PATH"

trap "killall -q -9 slurmd slurmctld slurmstepd slurmdbd srun submitter ticker job_runner" SIGINT SIGTERM EXIT

rm -Rf /dev/shm/*

TIME_STARTPAD=60
START_TIME="$(trace_list -n -w $1 | awk '{print $5;}' | sort -n | head -n 1)"
START_TIME="$(( $START_TIME - $TIME_STARTPAD ))"

TIME_ENDPAD=60
END_TIME="$(trace_list -n -w $1 | awk '{print $8;}' | sort -nr | head -n 1)"
END_TIME="$(( $END_TIME + $TIME_ENDPAD ))"

# Add initial time
ticker -s "$START_TIME"

# Initiate slurm and slurmdb
./start_slurm.sh "$SLURM_REPLAY/distime" "$SLURM_REPLAY_LIB"

echo "Slurm is configured and ready:"
echo
sinfo --summarize
echo

echo -n "Start submitter... "
rm -f submitter.log
submitter -w "$WORKLOAD" -t template.script
sleep 3
echo "done."

# Make time progress at a faster rate
RATE="0.1"
TICK="1"
DURATION=$(( $END_TIME - $START_TIME ))
END_REPLAY=$( echo "$DURATION*($RATE/$TICK)" | bc -l)
echo "Replay ends at $(date --date="${END_REPLAY%.*} seconds")"

ticker -c "$END_TIME,$RATE,$TICK"

date
echo -n "Collecting data... "
# get the replay trace
query=$(trace_list -w $WORKLOAD -q | head -n 1)
trace_builder_mysql -f "replay.$WORKLOAD" -u "maximem" -p "" -h "localhost" -d "slurm_acct_db" -q "$query" -t daint_job_table -r daint_resv_table
echo "done."
echo
echo "ERROR IF ANY:"
grep -E "\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\] error:" log/slurmctld.log log/slurmd/*.log submitter.log log/slurmdbd.log
