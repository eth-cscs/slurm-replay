#!/bin/bash


while getopts ":w:r:" opt; do
case $opt in
    w)
       WORKLOAD="$OPTARG"
    ;;
    r)
       RATE="$OPTARG"
    ;;
    :)
       echo "Option -$OPTARG requires an argument."
       exit 1
    ;;
esac
done

if [ -z "$WORKLOAD" ]; then
    echo "Please provide a trace file"
    exit 1
fi
if [ -z "$RATE" ]; then
    RATE="0.1"
fi
TICK="1"
CLOCK_RATE=$(echo "$RATE*$TICK" | bc -l)

TMP_DIR="/home/$REPLAY_USER/tmp"
SLURM_DIR="/home/$REPLAY_USER/slurmR"
SLURM_REPLAY="/home/$REPLAY_USER/slurm-replay"
SLURM_REPLAY_LIB="libwtime.so"

export PATH="$SLURM_REPLAY/submitter:$PATH"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_REPLAY/distime:$SLURM_DIR/lib:$LD_LIBRARY_PATH"

# Do not enable when using on a batch system, killing srun will kill the sbatch job
PROCESS_TOKILL="slurmd slurmctld slurmstepd slurmdbd submitter ticker job_runner node_controller"
killall -q -9 $PROCESS_TOKILL
trap "killall -q -9 $PROCESS_TOKILL" SIGINT SIGTERM EXIT

rm -Rf /dev/shm/ShmemClock*

TIME_STARTPAD=60
START_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $4;}' | sort -n | head -n 1)"
START_TIME="$(( $START_TIME - $TIME_STARTPAD ))"

TIME_ENDPAD=60
END_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $7;}' | sort -nr | head -n 1)"
END_TIME="$(( $END_TIME + $TIME_ENDPAD ))"

NJOBS="$(trace_list -n -w "$WORKLOAD" | wc -l)"

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
rm -f submitter.log node_controller.log "$TMP_DIR/accel_time"
touch "$TMP_DIR/accel_time"
submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE" -u "$REPLAY_USER" -m "$TMP_DIR/accel_time"
node_controller -w "$WORKLOAD"
sleep 3
echo "done."

# Make time progress at a faster rate
DURATION=$(( $END_TIME - $START_TIME ))
END_REPLAY=$( echo "$DURATION*($CLOCK_RATE)" | bc -l)
echo "Replay tentative ending time is $(date --date="${END_REPLAY%.*} seconds")"

ticker -c "$END_TIME,$RATE,$TICK" -n "$NJOBS" -a "$TMP_DIR/accel_time"

sleep 5
ticker -o -n "$NJOBS"
echo "current date: $(date)"

echo -n "Collecting data... "
# get the query and remove where close
query=$(trace_list -w $WORKLOAD -q | head -n 1)
query="${query%WHERE*};"
REPLAY_WORKLOAD="../data/replay.${WORKLOAD##*/}"
CT=0
while [ -f "$REPLAY_WORKLOAD.$CT" ]; do
    CT=$(( $CT + 1 ))
done
REPLAY_WORKLOAD="$REPLAY_WORKLOAD.$CT"
trace_builder_mysql -f "$REPLAY_WORKLOAD" -u "$REPLAY_USER" -p "" -h "localhost" -d "slurm_acct_db" -q "$query" -t daint_job_table -r daint_resv_table -v daint_event_table
echo "done."
echo
echo "ERROR IF ANY:"
LOGFILE="log/slurmctld.log log/slurmd/*.log log/submitter.log log/slurmdbd.log log/node_controller.log"
grep -E "\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\] error:" $LOGFILE &> ../data/error.log
cat ../data/error.log
cp $LOGFILE ../data

# Gather statistics from slurm
#echo "Slurm schedule statistics"
#echo "sreport"
#START_DATE=$(date -d @$START_TIME +%Y-%m-%dT%H:%M)
#END_DATE=$(date -d @$END_TIME +%Y-%m-%dT%H:%M)

