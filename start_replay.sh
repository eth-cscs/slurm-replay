#!/bin/bash

WORKLOAD="$1"
if [ -z "$WORKLOAD" ]; then
    echo "Please provide a trace file"
    exit 1
fi


SLURM_DIR="/home/slurm/slurmR"
SLURM_REPLAY="/home/slurm/slurm-replay"
SLURM_REPLAY_LIB="libwtime.so"

export PATH="$SLURM_REPLAY/submitter:$PATH"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_REPLAY/distime:$SLURM_DIR/lib:$LD_LIBRARY_PATH"

trap "killall -q -9 slurmd slurmctld slurmstepd slurmdbd srun submitter ticker job_runner node_controller" SIGINT SIGTERM EXIT

rm -Rf /dev/shm/*

TIME_STARTPAD=60
START_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $4;}' | sort -n | head -n 1)"
START_TIME="$(( $START_TIME - $TIME_STARTPAD ))"

TIME_ENDPAD=60
END_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $7;}' | sort -nr | head -n 1)"
END_TIME="$(( $END_TIME + $TIME_ENDPAD ))"

NJOBS="$(trace_list -n -w "$WORKLOAD" | wc -l)"

RATE="0.1"
TICK="1"
CLOCK_RATE=$(echo "$RATE*$TICK" | bc -l)

CONF_TIME="$(trace_list -n -w "$WORKLOAD" -u | awk '{print $4;}' | sort -n | head -n 1 | sed 's/2017/17/' | tr -d '-')"
# Add initial time
ticker -s "$START_TIME"

# Initiate slurm and slurmdb
./start_slurm.sh "$SLURM_REPLAY/distime" "$SLURM_REPLAY_LIB" "$CONF_TIME"

echo "Slurm is configured and ready:"
echo
sinfo --summarize
echo

echo -n "Start submitter and node controller... "
rm -f submitter.log node_controller.log
submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE"
node_controller -w "$WORKLOAD"
sleep 3
echo "done."

# Make time progress at a faster rate
DURATION=$(( $END_TIME - $START_TIME ))
END_REPLAY=$( echo "$DURATION*($CLOCK_RATE)" | bc -l)
echo "Replay ends at $(date --date="${END_REPLAY%.*} seconds")"

ticker -c "$END_TIME,$RATE,$TICK" -n "$NJOBS"

# let slurm process uncompleted data
sleep 60
ticker -o

date
echo -n "Collecting data... "
# get the replay trace
query=$(trace_list -w $WORKLOAD -q | head -n 1)
REPLAY_WORKLOAD="${WORKLOAD##*/}"
trace_builder_mysql -f "replay.$REPLAY_WORKLOAD" -u "slurm" -p "" -h "localhost" -d "slurm_acct_db" -q "$query" -t daint_job_table -r daint_resv_table -v daint_event_table
echo "done."
echo
echo "ERROR IF ANY:"
grep -E "\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\] error:" log/slurmctld.log log/slurmd/*.log submitter.log log/slurmdbd.log node_controller.log
