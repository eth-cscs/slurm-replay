#!/bin/bash

SLURM_FILE="etc/slurm.conf"
SLURMDBD_FILE="etc/slurmdbd.conf"

# Comment lines
token="BackupController[[:space:]]*= BackupAddr[[:space:]]*= Prolog[[:space:]]*= BurstBufferType[[:space:]]*= PrologSlurmctld[[:space:]]*= Epilog[[:space:]]*= EpilogSlurmctld[[:space:]]*= UnkillableStepProgram[[:space:]]*= JobSubmitPlugins[[:space:]]*= JobCompLoc[[:space:]]*= PrologFlags[[:space:]]*="
for t in $token; do
   sed -i -e "/$t/ s/^#*/#/" $SLURM_FILE
done

# Change the plugin types to none
sed -i -e "s/AuthType[[:space:]]*=.*/AuthType=auth\/none/" $SLURM_FILE
sed -i -e "s/CryptoType[[:space:]]*=.*/CryptoType=crypto\/none/" $SLURM_FILE
sed -i -e "s/JobContainerType[[:space:]]*=.*/JobContainerType=job_container\/none/" $SLURM_FILE
sed -i -e "s/ProctrackType[[:space:]]*=.*/ProctrackType=proctrack\/linuxproc/" $SLURM_FILE
sed -i -e "s/TaskPlugin[[:space:]]*=.*/TaskPlugin=task\/none/" $SLURM_FILE
sed -i -e "s/JobCompType[[:space:]]*=.*/JobCompType=jobcomp\/none/" $SLURM_FILE
sed -i -e "s/AcctGatherEnergyType[[:space:]]*=.*/AcctGatherEnergyType=acct_gather_energy\/none/" $SLURM_FILE
sed -i -e "s/AcctGatherType[[:space:]]*=.*/AcctGatherType=jobacct_gather\/none/" $SLURM_FILE

# Timeout value
sed -i -e "s/SlurmdTimeout[[:space:]]*=[[:digit:]]*/SlurmdTimeout=0/" $SLURM_FILE


# Debug level
sed -i -e "s/SlurmdDebug[[:space:]]*=.*/SlurmdDebug=error/" $SLURM_FILE
sed -i -e "s/SlurmctldDebug[[:space:]]*=.*/SlurmctldDebug=error/" $SLURM_FILE

# Set up to local hosts
sed -i -e "s/ControlMachine[[:space:]]*=.*/ControlMachine=localhost/" $SLURM_FILE
sed -i -e "/ControlMachine[[:space:]]*=/a\
ControlAddr=localhost\n\
SlurmdUser=$REPLAY_USER\
" $SLURM_FILE
sed -i -e "s/SlurmUser[[:space:]]*=.*/SlurmUser=$REPLAY_USER/" $SLURM_FILE

# Set up directories
sed -i -e "s/SlurmctldPidFile[[:space:]]*=.*/SlurmctldPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.pid/" $SLURM_FILE
sed -i -e "s/SlurmctldLogFile[[:space:]]*=.*/SlurmctldLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.log/" $SLURM_FILE
sed -i -e "s/SlurmdPidFile[[:space:]]*=.*/SlurmdPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmd.pid/" $SLURM_FILE
sed -i -e "s/SlurmdLogFile[[:space:]]*=.*/SlurmdLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmd\/slurmd.log/" $SLURM_FILE
sed -i -e "s/SlurmdSpoolDir[[:space:]]*=.*/SlurmdSpoolDir=\/$REPLAY_USER\/slurmR\/log/" $SLURM_FILE
sed -i -e "s/StateSaveLocation[[:space:]]*=.*/StateSaveLocation=\/$REPLAY_USER\/slurmR\/log\/state/" $SLURM_FILE

# Accounting
sed -i -e "s/AccountingStorageHost[[:space:]]*=.*/AccountingStorageHost=localhost/" $SLURM_FILE
sed -i -e "s/AccountingStorageBackupHost[[:space:]]*=.*/AccountingStorageBackupHost=localhost/" $SLURM_FILE
sed -i -e "/AccountingStorageBackupHost[[:space:]]*=/a\
AccountingStorageUser=$REPLAY_USER\n\
AccountingStoragePass=\"\" \
" $SLURM_FILE

# Add NodeAddr and NodeHostname
sed -i -e "/NodeName[[:space:]]*=/s/$/\ NodeAddr=localhost\ NodeHostname=localhost/" $SLURM_FILE
sed -i -e "/NodeName[[:space:]]*=/s/#/\ NodeAddr=localhost\ NodeHostname=localhost &/" $SLURM_FILE

# Other changes
sed -i -e '1,/^NodeName=.*/ {/^NodeName=.*/i\
# Adding frontend\
FrontendName=localhost FrontendAddr=localhost Port=7000
}' $SLURM_FILE
sed -i -e "/DebugFlags=/a\
PluginDir=/$REPLAY_USER/slurmR/lib/slurm\
" $SLURM_FILE

# configure SlurmDBD
sed -i -e "s/SlurmUser[[:space:]]*=.*/SlurmUser=$REPLAY_USER/" $SLURMDBD_FILE
sed -i -e "s/StorageUser[[:space:]]*=.*/StorageUser=$REPLAY_USER/" $SLURMDBD_FILE
sed -i -e "s/PidFile[[:space:]]*=.*/PidFile=\/$REPLAY_USER\/slurmR\/log\/slurmdbd.pid/" $SLURMDBD_FILE
sed -i -e "s/LogFile[[:space:]]*=.*/LogFile=\/$REPLAY_USER\/slurmR\/log\/slurmdbd.log/" $SLURMDBD_FILE
