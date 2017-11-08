#!/bin/bash

if [ ! -z "$1" ]; then
SLURM_REPLAY_LIB="LD_PRELOAD=$1"
fi

SLURM_DIR="/home/maximem/dev/github/slurm_simulator/slurm_newsim"

export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_DIR/lib"

killall -q -9 slurmd slurmctld slurmstepd slurmdbd srun

rm -Rf /tmp/slurm-*.out
rm -Rf $SLURM_DIR/tmp/*
mkdir $SLURM_DIR/tmp/state
mkdir $SLURM_DIR/tmp/slurmd
mkdir $SLURM_DIR/tmp/archive

./start_slurmdbd.sh

echo -n  "Starting slurm... "
# Initiate slurm
for k in $(seq 12 1 18); do
    $SLURM_REPLAY_LIB slurmd -N nid000$k
done
sleep 5
$SLURM_REPLAY_LIB slurmctld
echo "done."
