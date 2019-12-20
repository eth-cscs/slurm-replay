#!/bin/bash


while getopts ":w:r:n:p:c:x:" opt; do
case $opt in
    w)
       WORKLOAD="$OPTARG"
    ;;
    r)
       RATE="$OPTARG"
    ;;
    n)
       NAME="$OPTARG"
    ;;
    p)
       PRESET="$OPTARG"
    ;;
    c)
       SUBMITTER_RUNTIME="$OPTARG"
    ;;
    x)
       SUBMITTER_SWITCH="$OPTARG"
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
if [ -z "$PRESET" ]; then
    PRESET="1"
fi
if [ -z "$NAME" ]; then
    NAME="unknown"
fi
TICK="1"
CLOCK_RATE=$(echo "$RATE*$TICK" | bc -l)
if (( $PRESET == 0 )); then
    echo "Preset=$PRESET - No preset jobs (priority=0, hostlist=0, reservation=1, node_controller=1)"
elif (( $PRESET == 1 )); then
    echo "Preset=$PRESET - Preset jobs (priority=1, hostlist=0, reservation=1, node_controller=1)"
elif (( $PRESET == 2 )); then
    echo "Preset=$PRESET - All jobs (priority=1, hostlist=0, reservation=1, node_controller=1)"
elif (( $PRESET == 3 )); then
    echo "Preset=$PRESET - All jobs (priority=1, hostlist=1, reservation=0, node_controller=0)"
fi
echo "current date: $(date)"

TMP_DIR="/$REPLAY_USER/tmp"
SLURM_DIR="/$REPLAY_USER/slurmR"
SLURM_REPLAY="/$REPLAY_USER/slurm-replay"
SLURM_REPLAY_LIB="libwtime.so"

export PATH="$SLURM_REPLAY/submitter:$PATH"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_REPLAY/distime:$SLURM_DIR/lib:$LD_LIBRARY_PATH"

# Do not enable when using on a batch system, killing srun will kill the sbatch job
PROCESS_TOKILL="slurmd slurmctld slurmstepd slurmdbd submitter ticker job_runner node_controller"
killall -q -9 $PROCESS_TOKILL
trap "killall -q -9 $PROCESS_TOKILL" SIGTERM EXIT

rm -Rf /dev/shm/ShmemClock*

TIME_STARTPAD=300
START_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $5;}' | sort -n | head -n 1)"
STR_START_TIME=$(date -d @$START_TIME +'%Y-%m-%d %H:%M:%S')
START_TIME="$(( $START_TIME - $TIME_STARTPAD ))"

TIME_ENDPAD=300
END_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $8;}' | sort -nr | head -n 1)"
END_TIME="$(( $END_TIME + $TIME_ENDPAD ))"

NJOBS="$(trace_list -n -w "$WORKLOAD" | wc -l)"

CONF_TIME="$(trace_list -n -w "$WORKLOAD" -u | awk '{print $5;}' | sort -n | head -n 1 | tr -d '-' | cut -c 3-8)"
# Add initial time
ticker -s "$START_TIME"

# Initiate rest-shell access
./start_restshell.sh

# Initiate slurm and slurmdb
./start_slurm.sh -c "daint" -l "$SLURM_REPLAY/distime/$SLURM_REPLAY_LIB" -t "$CONF_TIME"

echo "Slurm is configured and ready:"
echo
sinfo --summarize
echo

echo -n "Start submitter and node controller... "
rm -f submitter.log node_controller.log
if [[ ! -z "$SUBMITTER_RUNTIME" ]]; then
    echo -n "Submitter using option -c $SUBMITTER_RUNTIME ..."
    submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE" -u "$REPLAY_USER" -p "$PRESET" -c "$SUBMITTER_RUNTIME"
fi
if [[ ! -z "$SUBMITTER_SWITCH" ]]; then
    echo -n "Submitter using option -x ..."
    submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE" -u "$REPLAY_USER" -p "$PRESET" -x "$SUBMITTER_SWITCH"
fi
if [[ -z "$SUBMITTER_RUNTIME" && -z "$SUBMITTER_SWITCH" ]]; then
    echo -n "Submitter using no special option ..."
    submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE" -u "$REPLAY_USER" -p "$PRESET"
fi
if (( $PRESET < 3 )); then
    node_controller -w "$WORKLOAD" -r "$CLOCK_RATE"
fi
sleep 3
echo "done."

# Start with a slow rate to let slurm process the preset jobs in the queue
#if (( $PRESET > 0)); then
#    echo -n "Let Slurm process the preset jobs... "
#    END_TIME_PRESET=$(( $START_TIME + $TIME_STARTPAD))
#    ticker -c "$END_TIME_PRESET,1,1"
#    echo "done."
#fi

# Make time progress at a faster rate
DURATION=$(( $END_TIME - $START_TIME ))
END_REPLAY=$( echo "$DURATION*($CLOCK_RATE)" | bc -l)
echo "Replay tentative ending time is $(date --date="${END_REPLAY%.*} seconds")"

ticker -c "$END_TIME,$RATE,$TICK" -n "$NJOBS"

sleep 5
ticker -o -n "$NJOBS"
sdiag -a
echo "current date: $(date)"
NJOBS_ACTIVE=$(($(squeue | wc -l) -1))
echo "njobs still active at the end of replay: $NJOBS_ACTIVE"
echo -n "Collecting data... "
# correct the db, Slurm bug: some time_start may not be filled in the job_table while they are correct in the step_table
# http://slurm-dev.schedmd.narkive.com/FkIMYBpQ/consistency-checks-and-missing-time-start-in-slurmdbd
# should we use sacctmgr runaway?
db_correctness -u "$REPLAY_USER" -p "" -h "localhost" -d "slurm_acct_db" -t daint_job_table -s daint_step_table
REPLAY_WORKLOAD_DIR="../data/replay.${WORKLOAD##*/}.$NAME.$CLOCK_RATE"
CT=0
while [ -d "$REPLAY_WORKLOAD_DIR.$CT" ]; do
    CT=$(( $CT + 1 ))
done
REPLAY_WORKLOAD_DIR="$REPLAY_WORKLOAD_DIR.$CT"
mkdir $REPLAY_WORKLOAD_DIR
REPLAY_WORKLOAD="$REPLAY_WORKLOAD_DIR/replay.${WORKLOAD##*/}"
trace_builder_mysql -f "$REPLAY_WORKLOAD" -u "$REPLAY_USER" -p "" -h "localhost" -d "slurm_acct_db"  -w -c daint -s "$STR_START_TIME" -n
echo "trace_builder_mysql -f \"$REPLAY_WORKLOAD\" -u \"$REPLAY_USER\" -p \"\" -h \"localhost\" -d \"slurm_acct_db\" -w -c daint -s \"$STR_START_TIME\" -n"
echo "done."
echo
echo "ERROR IF ANY:"
LOGFILE="log/slurmctld.log log/slurmd/*.log log/submitter.log log/slurmdbd.log log/node_controller.log"
grep -E "\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\] error:" $LOGFILE &> $REPLAY_WORKLOAD_DIR/error.log
cat $REPLAY_WORKLOAD_DIR/error.log
cp $LOGFILE $REPLAY_WORKLOAD_DIR
trace_metrics -w "$REPLAY_WORKLOAD" > $REPLAY_WORKLOAD_DIR/metrics.log
