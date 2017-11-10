#!/bin/bash

NNODES=50
#NNODES=$(cat all_nids.txt | wc -l)

if [ ! -z "$1" -a ! -z "$2" ]; then
   export LD_LIBRARY_PATH="$1:$LD_LIBRARY_PATH"
   SLURM_REPLAY_LIB="LD_PRELOAD=$2"
fi

SLURM_DIR="/home/maximem/dev/github/slurm_simulator/slurm_newsim"

export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_DIR/lib:$LD_LIBRARY_PATH"

killall -q -9 slurmd slurmctld slurmstepd slurmdbd srun

rm -Rf /tmp/slurm-*.out
rm -Rf $SLURM_DIR/tmp/*
mkdir $SLURM_DIR/tmp/state
mkdir $SLURM_DIR/tmp/slurmd
mkdir $SLURM_DIR/tmp/archive

./start_slurmdbd.sh

echo -n  "Starting slurmctld and $NNODES slurmd... "
nids="$(cat all_nids.txt | head -n $NNODES | xargs printf "nid%05d ")"
parallel eval "$SLURM_REPLAY_LIB slurmd -v -N {}" ::: $nids
sleep 5
eval "$SLURM_REPLAY_LIB slurmctld -v"
echo "done."
