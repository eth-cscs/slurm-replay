#!/bin/bash

SLURM_FILE="etc/slurm.conf"
SLURMDBD_FILE="etc/slurmdbd.conf"

# Comment lines
token="BackupController[[:space:]]*= BackupAddr[[:space:]]*= Prolog[[:space:]]*= BurstBufferType[[:space:]]*= PrologSlurmctld[[:space:]]*= Epilog[[:space:]]*= EpilogSlurmctld[[:space:]]*= UnkillableStepProgram[[:space:]]*= JobSubmitPlugins[[:space:]]*= JobCompLoc[[:space:]]*= PrologFlags[[:space:]]*= SwitchType[[:space:]]*= CoreSpecPlugin[[:space:]]*= SelectType[[:space:]]*= SelectTypeParameters[[:space:]]*="
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
sed -i -e "s/MessageTimeout[[:space:]]*=[[:digit:]]*/MessageTimeout=240/" $SLURM_FILE


# Debug level
sed -i -e "s/SlurmdDebug[[:space:]]*=.*/SlurmdDebug=error/" $SLURM_FILE
sed -i -e "s/SlurmctldDebug[[:space:]]*=.*/SlurmctldDebug=error/" $SLURM_FILE

# Set up to local hosts
sed -i -e "/^ControlAddr[[:space:]]*=.*/ d" $SLURM_FILE
sed -i -e "/^SlurmdUser[[:space:]]*=.*/ d" $SLURM_FILE
sed -i -e "s/^ControlMachine[[:space:]]*=.*/ControlMachine=localhost/" $SLURM_FILE
sed -i -e "/ControlMachine[[:space:]]*=/a\
ControlAddr=localhost\n\
SlurmdUser=$REPLAY_USER\
" $SLURM_FILE
sed -i -e "s/SlurmUser[[:space:]]*=.*/SlurmUser=$REPLAY_USER/" $SLURM_FILE

# Set up ports
if [ ! -z "$SLURMCTLD_PORT1" ]; then
sed -i -e "s/SlurmctldPort[[:space:]]*=.*/SlurmctldPort=${SLURMCTLD_PORT1}-${SLURMCTLD_PORT2}/" $SLURM_FILE
fi
if [ ! -z "$SLURMD_PORT" ]; then
sed -i -e "s/SlurmdPort[[:space:]]*=.*/SlurmdPort=$SLURMD_PORT/" $SLURM_FILE
fi
if [ ! -z "$SLURMDBD_PORT" ]; then
sed -i -e "s/DbdPort[[:space:]]*=.*/DbdPort=$SLURMDBD_PORT/" $SLURMDBD_FILE
sed -i -e "s/AccountingStoragePort[[:space:]]*=.*/AccountingStoragePort=$SLURMDBD_PORT/" $SLURM_FILE
fi
if [ ! -z "$MYSQL_PORT" ]; then
sed -i -e "s/StoragePort[[:space:]]*=.*/StoragePort=$MYSQL_PORT/" $SLURMDBD_FILE
fi

# No return to service of DOWN nodes
sed -i -e "s/ReturnToService[[:space:]]*=.*/ReturnToService=0/" $SLURM_FILE

# Set up directories
sed -i -e "s/SlurmctldPidFile[[:space:]]*=.*/SlurmctldPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.pid/" $SLURM_FILE
sed -i -e "s/SlurmctldLogFile[[:space:]]*=.*/SlurmctldLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.log/" $SLURM_FILE
sed -i -e "s/SlurmdPidFile[[:space:]]*=.*/SlurmdPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmd.pid/" $SLURM_FILE
sed -i -e "s/SlurmdLogFile[[:space:]]*=.*/SlurmdLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmd\/slurmd.log/" $SLURM_FILE
sed -i -e "s/SlurmdSpoolDir[[:space:]]*=.*/SlurmdSpoolDir=\/$REPLAY_USER\/slurmR\/log/" $SLURM_FILE
sed -i -e "s/StateSaveLocation[[:space:]]*=.*/StateSaveLocation=\/$REPLAY_USER\/slurmR\/log\/state/" $SLURM_FILE
sed -i -e "s/SlurmSchedLogFile[[:space:]]*=.*/SlurmSchedLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld_sched.log/" $SLURM_FILE

# Accounting
sed -i -e "s/AccountingStorageHost[[:space:]]*=.*/AccountingStorageHost=localhost/" $SLURM_FILE
sed -i -e "s/AccountingStorageBackupHost[[:space:]]*=.*/AccountingStorageBackupHost=localhost/" $SLURM_FILE
sed -i -e "/AccountingStorageBackupHost[[:space:]]*=/a\
AccountingStorageUser=$REPLAY_USER\n\
AccountingStoragePass=\"\" \
" $SLURM_FILE

# Add NodeAddr and NodeHostname
sed -i -e "/^NodeName[[:space:]]*=/s/NodeHostname=.* /NodeHostname=localhost /" $SLURM_FILE
sed -i -e "/^NodeName[[:space:]]*=/s/NodeAddr=.* /NodeAddr=localhost /" $SLURM_FILE
sed -i -e "/^NodeName[[:space:]]*=.*NodeAddr=.*/{p;d}; /^NodeName[[:space:]]*=/s/$/\ NodeAddr=localhost/" $SLURM_FILE
sed -i -e "/^NodeName[[:space:]]*=.*NodeHostname=.*/{p;d}; /^NodeName[[:space:]]*=/s/$/\ NodeHostname=localhost/" $SLURM_FILE
sed -i -e "/^NodeName[[:space:]]*=/s/#.*/\ NodeAddr=localhost\ NodeHostname=localhost/" $SLURM_FILE

# Other changes
sed -i -e "1,/^NodeName=.*/ {/^NodeName=.*/i\
FrontendName=localhost FrontendAddr=localhost Port=$FRONTEND_PORT
}" $SLURM_FILE
sed -i -e "/DebugFlags=/a\
PluginDir=/$REPLAY_USER/slurmR/lib/slurm\
" $SLURM_FILE

# configure SlurmDBD
sed -i -e "s/SlurmUser[[:space:]]*=.*/SlurmUser=$REPLAY_USER/" $SLURMDBD_FILE
sed -i -e "s/StorageUser[[:space:]]*=.*/StorageUser=$REPLAY_USER/" $SLURMDBD_FILE
sed -i -e "s/PidFile[[:space:]]*=.*/PidFile=\/$REPLAY_USER\/slurmR\/log\/slurmdbd.pid/" $SLURMDBD_FILE
sed -i -e "s/LogFile[[:space:]]*=.*/LogFile=\/$REPLAY_USER\/slurmR\/log\/slurmdbd.log/" $SLURMDBD_FILE
