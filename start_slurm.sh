#!/bin/bash

#VERBOSE="-v"

while getopts ":c:l:t:" opt; do
case $opt in
    c)
       CLUSTER="$OPTARG"
    ;;
    l)
       LIBNAME=$(basename $OPTARG)
       LIBDIR=$(dirname $OPTARG)
       export LD_LIBRARY_PATH="$LIBDIR:$LD_LIBRARY_PATH"
       SLURM_REPLAY_LIB="LD_PRELOAD=$LIBNAME"
    ;;
    t)
       CONFDATE="$OPTARG"
    ;;
    :)
       echo "Option -$OPTARG requires an argument."
       exit 1
    ;;
esac
done

if [ -z "$CLUSTER" ]; then
   CLUSTER="daint"
fi
if [ -z "$CONFDATE" ]; then
   CONFDATE=$(date '+%y%m%d')
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
if [ -f "../data/slurmcfg/.git/config" ]; then
   echo "Slurm configuration from git:"
   cd ../data/slurmcfg
   t=20${CONFDATE:0:2}-${CONFDATE:2:2}-${CONFDATE:4:2}
   target_revision=$(git rev-list -1 --until="$t" $CLUSTER)
   current_revision=$(git rev-parse HEAD)
   if [ "$target_revision" != "$current_revision" ]; then
      git checkout "$target_revision"
   else
      echo "using current revision: $current_revision"
   fi
   echo "done."
fi
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
if [ -f "../data/extra_command_slurm.sh" ]; then
   ../data/./extra_command_slurm.sh
else
   nodes=$(sinfo -o %N --noheader)
   scontrol update NodeName=$nodes state=DOWN Reason="complete slurm replay setup"
   scontrol update NodeName=$nodes state=RESUME Reason="complete slurm replay setup"
   scontrol update FrontEnd=localhost state=RESUME Reason="complete slurm replay setup"
   partitions=$(sinfo -o %R --noheader)
   for p in $partitions; do
       scontrol update PartitionName=$p state=UP
   done;
fi
sacctmgr -i create user name=$REPLAY_USER cluster=$CLUSTER account=root
echo "done."
