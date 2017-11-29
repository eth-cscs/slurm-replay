#!/bin/bash

VERBOSE="-v"

if [ ! -z "$1" -a ! -z "$2" ]; then
   export LD_LIBRARY_PATH="$1:$LD_LIBRARY_PATH"
   SLURM_REPLAY_LIB="LD_PRELOAD=$2"
fi

SLURM_DIR="/home/slurm/slurmR"

export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_DIR/lib:$LD_LIBRARY_PATH"

killall -q -9 slurmd slurmctld slurmstepd srun

rm -Rf /tmp/slurm-*.out
rm -Rf $SLURM_DIR/log/*
mkdir $SLURM_DIR/log/state
mkdir $SLURM_DIR/log/slurmd
mkdir $SLURM_DIR/log/archive

./start_slurmdbd.sh

# With multiple_slurmd
#NNODES=$(cat all_nids.txt | wc -l)
#echo -n  "Starting slurmctld and $NNODES slurmd... "
#nids="$(cat all_nids.txt | head -n $NNODES | xargs printf "nid%05d ")"
#parallel eval "$SLURM_REPLAY_LIB slurmd $VERBOSE -c -N {}" ::: $nids
#sleep 15
#eval "$SLURM_REPLAY_LIB slurmctld $VERBOSE -c "
#sleep 15

# With front-end
echo -n  "Starting slurmctld and slurmd... "
eval "$SLURM_REPLAY_LIB slurmd $VERBOSE -c "
sleep 2
eval "$SLURM_REPLAY_LIB slurmctld $VERBOSE -c "
sleep 5
nodes=$(sinfo -o %N --noheader)
scontrol update NodeName=$nodes state=DOWN Reason="complete slurm replay setup"
scontrol update NodeName=$nodes state=RESUME Reason="complete slurm replay setup"
scontrol update FrontEnd=localhost state=RESUME Reason="complete slurm replay setup"
echo "done."
