Slurm Replay allows replaying traces of job scheduled on Daint.

Start slurm-replay in a container on Dom:
```
salloc -N 1 -C mc --time 05:00:00
export PATH=$PATH:/apps/dom/SI/opt/shifter/mount_option/bin
shifter --image mmxcscs/slurm-replay:maximem_slurm-17.02.9 --writable-volatile /home/maximem/slurmR/log --writable-volatile /home/maximem/slurmR/etc  --volume /users/maximem/dev/data:/home/maximem/data /bin/bash
```
