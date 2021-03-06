#!/bin/bash


while getopts ":w:r:n:p:c:x:P:s:" opt; do
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
    P)
       REPLAY_PORT="$OPTARG"
    ;;
    s)
       REPLAY_STARTTIME="$OPTARG"
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
    PRESET="1,1,0,0"
fi
if [ -z "$NAME" ]; then
    NAME="unknown"
fi
export FRONTEND_PORT=7000
if [ ! -z "$REPLAY_PORT" ]; then
    export RESTSHELL_PORT=$(( $REPLAY_PORT +1 ))
    export MYSQL_PORT=$(( $REPLAY_PORT +2 ))
    export SLURMCTLD_PORT1=$(( $REPLAY_PORT +3 ))
    export SLURMCTLD_PORT2=$(( $REPLAY_PORT +4 ))
    export SLURMD_PORT=$(( $REPLAY_PORT +5 ))
    export SLURMDBD_PORT=$(( $REPLAY_PORT +6 ))
    export FRONTEND_PORT=$(( $REPLAY_PORT +7 ))
fi

#set up passwd and group
if [ -f "/$REPLAY_USER/data/${WORKLOAD}_etc_passwd" ]; then
cp "/$REPLAY_USER/data/${WORKLOAD}_etc_passwd" /etc/passwd
cp "/$REPLAY_USER/data/${WORKLOAD}_etc_group" /etc/group
fi

TICK="1"
CLOCK_RATE=$(echo "$RATE*$TICK" | bc -l)
RSV_ENABLE=$(echo $PRESET | cut -d ',' -f 1)
NODECONTROLER_ENABLE=$(echo $PRESET | cut -d ',' -f 2)
PRIO_ENABLE=$(echo $PRESET | cut -d ',' -f 3)
NODELIST_ENABLE=$(echo $PRESET | cut -d ',' -f 4)
PRESET_VAL="$((2#${RSV_ENABLE}${NODECONTROLER_ENABLE}${PRIO_ENABLE}${NODELIST_ENABLE}))"
echo "Preset=${PRESET_VAL} - reservation=${RSV_ENABLE}, node_controller=${NODECONTROLER_ENABLE}, priority=${PRIO_ENABLE}, nodelist=${NODELIST_ENABLE}"

echo "current date: $(date)"

TMP_DIR="/$REPLAY_USER/tmp"
SLURM_DIR="/$REPLAY_USER/slurmR"
SLURM_REPLAY="/$REPLAY_USER/slurm-replay"
SLURM_REPLAY_LIB="libwtime.so"

export PATH="$SLURM_REPLAY/submitter:$SLURM_REPLAY/tracetools:$PATH"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_REPLAY/distime:$SLURM_DIR/lib:$LD_LIBRARY_PATH"

REPLAY_WORKLOAD_DIR="../data/replay.${WORKLOAD##*/}.$NAME.$CLOCK_RATE"
CT=0
while [ -d "$REPLAY_WORKLOAD_DIR.$CT" ]; do
    CT=$(( $CT + 1 ))
done
REPLAY_WORKLOAD_DIR="$REPLAY_WORKLOAD_DIR.$CT"
mkdir $REPLAY_WORKLOAD_DIR

finalize() {
   LOGFILE="log/slurmctld.log log/slurmd/*.log log/submitter.log log/slurmdbd.log log/node_controller.log"
   grep -E "\[[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\] error:" $LOGFILE &> $REPLAY_WORKLOAD_DIR/error.log
   cat $REPLAY_WORKLOAD_DIR/error.log
   cp $LOGFILE $REPLAY_WORKLOAD_DIR
   trap - SIGTERM EXIT
   # Do not kill srun
   PROCESS_TOKILL="slurmd slurmctld slurmstepd slurmdbd submitter ticker job_runner node_controller"
   killall -q -9 $PROCESS_TOKILL
}
trap finalize SIGTERM EXIT


if [ -z "$DISTIME_SHMEMCLOCK_NAME" ]; then
rm -Rf /dev/shm/ShmemClock*
export DISTIME_SHMEMCLOCK_NAME="/ShmemClock"
else
rm -Rf /dev/shm/${DISTIME_SHMEMCLOCK_NAME}*
fi

TIME_STARTPAD=300
if [ -z "$REPLAY_STARTTIME" ]; then
   START_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $5;}' | sort -n | head -n 1)"
else
   START_TIME="$REPLAY_STARTTIME"
fi
STR_START_TIME=$(date -d @$START_TIME +'%Y-%m-%d %H:%M:%S')
START_TIME="$(( $START_TIME - $TIME_STARTPAD ))"

TIME_ENDPAD=300
END_TIME="$(trace_list -n -w "$WORKLOAD" | awk '{print $8;}' | sort -nr | head -n 1)"
END_TIME="$(( $END_TIME + $TIME_ENDPAD ))"

NJOBS="$(trace_list -n -w "$WORKLOAD" | wc -l)"

CONF_TIME="$(trace_list -n -w "$WORKLOAD" -u | awk '{print $5;}' | sort -n | head -n 1 | tr -d '-' | cut -c 3-8)"
# Add initial time

ticker -s "$START_TIME"
echo "current replay date: $(ticker -g)"

# Initiate rest-shell access
./start_restshell.sh

# Initiate slurm and slurmdb
./start_slurm.sh -c "daint" -l "$SLURM_REPLAY/distime/$SLURM_REPLAY_LIB" -t "$CONF_TIME"

echo "Slurm is configured and ready:"
echo
sinfo --summarize
echo

echo -n "Start submitter... "
rm -f log/submitter.log log/node_controller.log
if [[ ! -z "$SUBMITTER_RUNTIME" ]]; then
    echo -n "Submitter using option -c $SUBMITTER_RUNTIME ..."
    submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE" -u "$REPLAY_USER" -p "$PRESET_VAL" -c "$SUBMITTER_RUNTIME"
fi
if [[ ! -z "$SUBMITTER_SWITCH" ]]; then
    echo -n "Submitter using option -x ..."
    submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE" -u "$REPLAY_USER" -p "$PRESET_VAL" -x "$SUBMITTER_SWITCH"
fi
if [[ -z "$SUBMITTER_RUNTIME" && -z "$SUBMITTER_SWITCH" ]]; then
    submitter -w "$WORKLOAD" -t template.script -r "$CLOCK_RATE" -u "$REPLAY_USER" -p "$PRESET_VAL"
fi
echo "done."
if [[ "$NODECONTROLER_ENABLE" == "1" ]]; then
    echo -n "Start node controller... "
    node_controller -w "$WORKLOAD" -r "$CLOCK_RATE"
    echo "done."
else
    echo "Node controller not started." > log/node_controller.log
fi
sleep 3

# extra action before to start like blocking until an event is triggered
if [ -f "../data/extra_start_action.sh" ]; then
  ../data/./extra_start_action.sh
fi

# Make time progress at a faster rate
DURATION=$(( $END_TIME - $START_TIME ))
END_REPLAY=$( echo "$DURATION*($CLOCK_RATE)" | bc -l)
echo "Replay tentative ending time is $(date --date="${END_REPLAY%.*} seconds")"

ticker -c "$END_TIME,$RATE,$TICK" -n "$NJOBS"

# extra action at the end like blocking until an event is triggered
if [ -f "../data/extra_end_action.sh" ]; then
  # continue to make the clock tick
  ticker -c "$END_TIME,$RATE,$TICK" -i
  ../data/./extra_end_action.sh
fi

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
REPLAY_WORKLOAD="$REPLAY_WORKLOAD_DIR/replay.${WORKLOAD##*/}"
trace_builder_mysql -f "$REPLAY_WORKLOAD" -u "$REPLAY_USER" -p "" -h "localhost" -d "slurm_acct_db"  -w -c daint -s "$STR_START_TIME" -n
echo "trace_builder_mysql -f \"$REPLAY_WORKLOAD\" -u \"$REPLAY_USER\" -p \"\" -h \"localhost\" -d \"slurm_acct_db\" -w -c daint -s \"$STR_START_TIME\" -n"
python tracetools/./wl_to_pickle.py -wl "${REPLAY_WORKLOAD}"
echo "done."
echo
echo "ERROR IF ANY:"
finalize
trace_metrics -w "$REPLAY_WORKLOAD" > $REPLAY_WORKLOAD_DIR/metrics.log
