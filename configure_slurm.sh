#!/bin/bash

FILE="$1"

# Comment lines
token="BackupController[[:space:]]*= BackupAddr[[:space:]]*= Prolog[[:space:]]*= BurstBufferType[[:space:]]*= PrologSlurmctld[[:space:]]*= Epilog[[:space:]]*= EpilogSlurmctld[[:space:]]*= UnkillableStepProgram[[:space:]]*= JobSubmitPlugins[[:space:]]*= JobCompLoc[[:space:]]*="
for t in $token; do
   sed -i -e "/$t/ s/^#*/#/" $FILE
done

# Change the plugin types to none
sed -i -e "/AuthType[[:space:]]*=/ s/munge/none/" $FILE
sed -i -e "/CryptoType[[:space:]]*=/ s/munge/none/" $FILE
sed -i -e "/JobContainerType[[:space:]]*=/ s/cncu/none/" $FILE
sed -i -e "/ProctrackType[[:space:]]*=/ s/cray/linuxproc/" $FILE
sed -i -e "/TaskPlugin[[:space:]]*=/ s/task.*/task\/none/" $FILE
sed -i -e "/JobCompType[[:space:]]*=/ s/jobcomp.*/jobcomp\/none/" $FILE
sed -i -e "/AcctGatherEnergyType[[:space:]]*=/ s/cray/none/" $FILE
sed -i -e "/AcctGatherType[[:space:]]*=/ s/linux/none/" $FILE

# TImeout value
sed -i -e "/SlurmdTimeout[[:space:]]*=/ s/SlurmdTimeout[[:space:]]*=[[:digit:]]*/SlurmdTimeout=0/" $FILE

# Schedule parameters
#sed -i -e "/SchedulerParameters[[:space:]]*=/ s/default_queue_depth=[[:digit:]]*/default_queue_depth=30000/" $FILE
#sed -i -e "/SchedulerParameters[[:space:]]*=/ s/max_rpc_cnt=[[:digit:]]*/max_rpc_cnt=3000/" $FILE
#sed -i -e "/SchedulerParameters[[:space:]]*=/ s/$/,bf_max_time=900,pack_serial_at_end,bf_yield_sleep=1000000/" $FILE

# Debug level
sed -i -e "/SlurmdDebug[[:space:]]*=/ s/info/error/" $FILE
sed -i -e "/SlurmctldDebug[[:space:]]*=/ s/debug/error/" $FILE

# Set up to local hosts
sed -i -e "/ControlMachine[[:space:]]*=/ s/daint.*/localhost/" $FILE
sed -i -e "/ControlMachine[[:space:]]*=/a\
ControlAddr=localhost\n\
SlurmdUser=$REPLAY_USER\
" $FILE
sed -i -e "s/SlurmUser[[:space:]]*=root/SlurmUser=$REPLAY_USER/" $FILE

# Set up directories
sed -i -e "/SlurmctldPidFile[[:space:]]*=/ s/SlurmctldPidFile[[:space:]]*=.*/SlurmctldPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.pid/" $FILE
sed -i -e "/SlurmctldLogFile[[:space:]]*=/ s/SlurmctldLogFile[[:space:]]*=.*/SlurmctldLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmctld.log/" $FILE
sed -i -e "/SlurmdPidFile[[:space:]]*=/ s/SlurmdPidFile[[:space:]]*=.*/SlurmdPidFile=\/$REPLAY_USER\/slurmR\/log\/slurmd.pid/" $FILE
sed -i -e "/SlurmdLogFile[[:space:]]*=/ s/SlurmdLogFile[[:space:]]*=.*/SlurmdLogFile=\/$REPLAY_USER\/slurmR\/log\/slurmd\/slurmd.log/" $FILE
sed -i -e "/SlurmdSpoolDir[[:space:]]*=/ s/SlurmdSpoolDir[[:space:]]*=.*/SlurmdSpoolDir=\/$REPLAY_USER\/slurmR\/log/" $FILE
sed -i -e "/StateSaveLocation[[:space:]]*=/ s/StateSaveLocation[[:space:]]*=.*/StateSaveLocation=\/$REPLAY_USER\/slurmR\/log\/state/" $FILE

# Accounting
sed -i -e "/AccountingStorageHost[[:space:]]*=/ s/AccountingStorageHost[[:space:]]*=.*/AccountingStorageHost=localhost/" $FILE
sed -i -e "/AccountingStorageBackupHost[[:space:]]*=/ s/AccountingStorageBackupHost[[:space:]]*=.*/AccountingStorageBackupHost=localhost/" $FILE
sed -i -e "/AccountingStorageBackupHost[[:space:]]*=/a\
AccountingStorageUser=$REPLAY_USER\n\
AccountingStoragePass=\"\" \
" $FILE

# Add NodeAddr and NodeHostname
sed -i -e "/NodeName[[:space:]]*=/s/$/\ NodeAddr=localhost\ NodeHostname=localhost/" $FILE
sed -i -e "/NodeName[[:space:]]*=/s/#/\ NodeAddr=localhost\ NodeHostname=localhost &/" $FILE

# Other changes
sed -i -e '/#\ XC\ NODES\ #####/a\
# Adding frontend\
FrontendName=localhost FrontendAddr=localhost Port=7000
' $FILE
sed -i -e "/DebugFlags=/a\
PluginDir=/$REPLAY_USER/slurmR/lib/slurm\
" $FILE
