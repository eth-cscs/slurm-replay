#!/bin/bash

SLURM_FILE="etc/slurm.conf"
SLURMDDB_FILE="etc/slurmdbd.conf"

# Comment lines
token="BackupController[[:space:]]*= BackupAddr[[:space:]]*= Prolog[[:space:]]*= BurstBufferType[[:space:]]*= PrologSlurmctld[[:space:]]*= Epilog[[:space:]]*= EpilogSlurmctld[[:space:]]*= UnkillableStepProgram[[:space:]]*= JobSubmitPlugins[[:space:]]*= JobCompLoc[[:space:]]*="
for t in $token; do
   sed -i -e "/$t/ s/^#*/#/" $SLURM_FILE
done

# Change the plugin types to none
sed -i -e "/AuthType[[:space:]]*=/ s/munge/none/" $SLURM_FILE
sed -i -e "/CryptoType[[:space:]]*=/ s/munge/none/" $SLURM_FILE
sed -i -e "/JobContainerType[[:space:]]*=/ s/cncu/none/" $SLURM_FILE
sed -i -e "/ProctrackType[[:space:]]*=/ s/cray/linuxproc/" $SLURM_FILE
sed -i -e "/TaskPlugin[[:space:]]*=/ s/task.*/task\/none/" $SLURM_FILE
sed -i -e "/JobCompType[[:space:]]*=/ s/jobcomp.*/jobcomp\/none/" $SLURM_FILE
sed -i -e "/AcctGatherEnergyType[[:space:]]*=/ s/cray/none/" $SLURM_FILE
sed -i -e "/AcctGatherType[[:space:]]*=/ s/linux/none/" $SLURM_FILE

# Timeout value
sed -i -e "/SlurmdTimeout[[:space:]]*=/ s/SlurmdTimeout[[:space:]]*=[[:digit:]]*/SlurmdTimeout=0/" $SLURM_FILE


# Debug level
sed -i -e "/SlurmdDebug[[:space:]]*=/ s/info/error/" $SLURM_FILE
sed -i -e "/SlurmctldDebug[[:space:]]*=/ s/debug/error/" $SLURM_FILE

# Set up to local hosts
sed -i -e "/ControlMachine[[:space:]]*=/ s/daint.*/localhost/" $SLURM_FILE
sed -i -e "/ControlMachine[[:space:]]*=/a\
ControlAddr=localhost\n\
SlurmdUser=$REPLAY_USER\
" $SLURM_FILE
sed -i -e "s/SlurmUser[[:space:]]*=root/SlurmUser=$REPLAY_USER/" $SLURM_FILE

# Set up directories
sed -i -e "/SlurmctldPidFile[[:space:]]*=/ s/SlurmctldPidFile[[:space:]]*=.*/SlurmctldPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.pid/" $SLURM_FILE
sed -i -e "/SlurmctldLogFile[[:space:]]*=/ s/SlurmctldLogFile[[:space:]]*=.*/SlurmctldLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.log/" $SLURM_FILE
sed -i -e "/SlurmdPidFile[[:space:]]*=/ s/SlurmdPidFile[[:space:]]*=.*/SlurmdPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmd.pid/" $SLURM_FILE
sed -i -e "/SlurmdLogFile[[:space:]]*=/ s/SlurmdLogFile[[:space:]]*=.*/SlurmdLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmd\/slurmd.log/" $SLURM_FILE
sed -i -e "/SlurmdSpoolDir[[:space:]]*=/ s/SlurmdSpoolDir[[:space:]]*=.*/SlurmdSpoolDir=\/$REPLAY_USER\/slurmR\/log/" $SLURM_FILE
sed -i -e "/StateSaveLocation[[:space:]]*=/ s/StateSaveLocation[[:space:]]*=.*/StateSaveLocation=\/$REPLAY_USER\/slurmR\/log\/state/" $SLURM_FILE

# Accounting
sed -i -e "/AccountingStorageHost[[:space:]]*=/ s/AccountingStorageHost[[:space:]]*=.*/AccountingStorageHost=localhost/" $SLURM_FILE
sed -i -e "/AccountingStorageBackupHost[[:space:]]*=/ s/AccountingStorageBackupHost[[:space:]]*=.*/AccountingStorageBackupHost=localhost/" $SLURM_FILE
sed -i -e "/AccountingStorageBackupHost[[:space:]]*=/a\
AccountingStorageUser=$REPLAY_USER\n\
AccountingStoragePass=\"\" \
" $SLURM_FILE

# Add NodeAddr and NodeHostname
sed -i -e "/NodeName[[:space:]]*=/s/$/\ NodeAddr=localhost\ NodeHostname=localhost/" $SLURM_FILE
sed -i -e "/NodeName[[:space:]]*=/s/#/\ NodeAddr=localhost\ NodeHostname=localhost &/" $SLURM_FILE

# Other changes
sed -i -e '/#\ XC\ NODES\ #####/a\
# Adding frontend\
FrontendName=localhost FrontendAddr=localhost Port=7000
' $SLURM_FILE
sed -i -e "/DebugFlags=/a\
PluginDir=/$REPLAY_USER/slurmR/lib/slurm\
" $SLURM_FILE

# configure SlurmDBD
sed -i -e "s/SlurmUser[[:space:]]*=slurm/SlurmUser=$REPLAY_USER/" $SLURMDBD_FILE
sed -i -e "s/StorageUser[[:space:]]*=slurm/StorageUser=$REPLAY_USER/" $SLURMDBD_FILE
sed -i -e "/PidFile[[:space:]]*=/ s/PidFile[[:space:]]*=.*/PidFile=\/$REPLAY_USER\/slurmR\/log\/slurmdbd.pid/" $SLURMDBD_FILE
sed -i -e "/LogFile[[:space:]]*=/ s/LogFile[[:space:]]*=.*/LogFile=\/$REPLAY_USER\/slurmR\/log\/slurmdbd.log/" $SLURMDBD_FILE
