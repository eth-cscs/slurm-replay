#!/bin/bash

#VERBOSE="-v"
CLUSTER="daint"

if [ ! -z "$1" -a ! -z "$2" ]; then
   export LD_LIBRARY_PATH="$1:$LD_LIBRARY_PATH"
   SLURM_REPLAY_LIB="LD_PRELOAD=$2"
   if [ ! -z "$3" ]; then
       CONFDATE="$3"
    fi
elif [ ! -z "$1" -a -z "$2" ]; then
       CONFDATE="$1"
fi

SLURM_DIR="/$REPLAY_USER/slurmR"

export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_DIR/lib:$LD_LIBRARY_PATH"

# Do not enable when using on a batch system, killing srun will kill the sbatch job
killall -q -9 slurmd slurmctld slurmstepd

rm -Rf /$REPLAY_USER/tmp/slurm-*.out
rm -Rf $SLURM_DIR/log
mkdir -p $SLURM_DIR/log/state
mkdir -p $SLURM_DIR/log/slurmd
mkdir -p $SLURM_DIR/log/archive

# Setup configuration
echo "Slurm configuration from git:"
cd ../data/slurmcfg
# TODO check if we are not already at the right revision
t=20${CONFDATE:0:2}-${CONFDATE:2:2}-${CONFDATE:4:2}
git checkout $(git rev-list -1 --until="$t" $CLUSTER)
cd /$REPLAY_USER/slurm-replay
cp "../data/slurmcfg/slurm.conf" etc/slurm.conf
cp "../data/slurmcfg/gres.conf" etc/gres.conf
cp "../data/slurmcfg/topology.conf" etc/topology.conf
if [ -f "../data/slurmcfg/slurmdbd.conf" ]; then
  cp "../data/slurmcfg/slurmdbd.conf" etc/slurmdbd.conf
else
  cp "../data/slurmdbd.conf" etc/slurmdbd.conf
fi
./configure_slurm.sh etc/slurm.conf
if [ -f "../data/extra_configure_slurm.sh" ]; then
    ../data/./extra_configure_slurm.sh
fi

./start_slurmdbd.sh $SLURM_REPLAY_LIB

# With front-end
echo -n  "Starting slurmctld and slurmd... "
eval "$SLURM_REPLAY_LIB slurmd $VERBOSE -c "
sleep 10
eval "$SLURM_REPLAY_LIB slurmctld $VERBOSE -c "
sleep 30
nodes=$(sinfo -o %N --noheader)
scontrol update NodeName=$nodes state=DOWN Reason="complete slurm replay setup"
scontrol update NodeName=$nodes state=RESUME Reason="complete slurm replay setup"
scontrol update FrontEnd=localhost state=RESUME Reason="complete slurm replay setup"
partitions=$(sinfo -o %R --noheader)
for p in $partitions; do
    scontrol update PartitionName=$p state=UP
done;
if [ -f "../data/extra_command_slurm.sh" ]; then
   ../data/./extra_command_slurm.sh
fi
echo "done."
