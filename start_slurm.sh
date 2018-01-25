#!/bin/bash

#VERBOSE="-v"

if [ ! -z "$1" -a ! -z "$2" ]; then
   export LD_LIBRARY_PATH="$1:$LD_LIBRARY_PATH"
   SLURM_REPLAY_LIB="LD_PRELOAD=$2"
   if [ ! -z "$3" ]; then
       CONFDATE="$3"
    fi
elif [ ! -z "$1" -a -z "$2" ]; then
       CONFDATE="$1"
fi

SLURM_DIR="/home/$REPLAY_USER/slurmR"

export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"
export LD_LIBRARY_PATH="$SLURM_DIR/lib:$LD_LIBRARY_PATH"

# Do not enable when using on a batch system, killing srun will kill the sbatch job
killall -q -9 slurmd slurmctld slurmstepd

rm -Rf /home/$REPLAY_USER/tmp/slurm-*.out
rm -Rf $SLURM_DIR/log/*
mkdir $SLURM_DIR/log/state
mkdir $SLURM_DIR/log/slurmd
mkdir $SLURM_DIR/log/archive

# Setup configuration
if (( $CONFDATE < 170901 )); then
    echo "!!! WARNING using old version of slurm config files !!!"
    f=0
    for k in $(ls -1 conf/slurm.conf_*); do
        t=${k##*_}
        if (( $t < $CONFDATE )); then
        f=$t
        else
        if (( $f == 0 )); then
            f=$t
        fi
        break
        fi
    done
    cp "conf/slurm.conf_$f" etc/slurm.conf
    ./configure_slurm.sh etc/slurm.conf

    f=0
    for k in $(ls -1 conf/gres.conf_*); do
        t=${k##*_}
        if (( $t < $CONFDATE )); then
        f=$t
        else
        if (( $f == 0 )); then
            f=$t
        fi
        break
        fi
    done
    cp "conf/gres.conf_$f" etc/gres.conf

    f=0
    for k in $(ls -1 conf/topology.conf_*); do
        t=${k##*_}
        if (( $t < $CONFDATE )); then
        f=$t
        else
        if (( $f == 0 )); then
            f=$t
        fi
        break
        fi
    done
    cp "conf/topology.conf_$f" etc/topology.conf
else
    echo "Slurm configuration from git:"
    cd ../data/slurmcfg
    git checkout daint &>/dev/null
    t=20${CONFDATE:0:2}-${CONFDATE:2:2}-${CONFDATE:4:2}
    git checkout $(git rev-list -1 --until="$t" daint)
    cd /home/$REPLAY_USER/slurm-replay
    cp "../data/slurmcfg/slurm.conf" etc/slurm.conf
    cp "../data/slurmcfg/gres.conf" etc/gres.conf
    cp "../data/slurmcfg/topology.conf" etc/topology.conf
    ./configure_slurm.sh etc/slurm.conf
fi

./start_slurmdbd.sh

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
partitions=$(sinfo -o %R --noheader)
for p in $partitions; do
    scontrol update PartitionName=$p state=UP
done;
echo "done."
