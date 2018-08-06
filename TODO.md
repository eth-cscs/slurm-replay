For more information on the Slurm-Replay and a technical description please refer to the SC'18 paper.

## Submitter specfic section of code:

A part of the code of the submitter is written to match a slurm configuration of Piz Daint at CSCS.
Piz Daint has multicore nodes and hybrid nodes with GPU. Selecting the type of node is done with a contraint.
Therefore, to identify the constraint the trace hold the `gres_alloc` of the job. Such check is specific to Piz Daint.
To solve that issue, one should identify the contraint at the creation of the workload and thus making `trace_builder_mysql` cluster dependent.
Unless more engineering is developed to make the constraint generic like providing a file as a parameter that associated constraints with values found inside `gres_alloc`.

## Anonymizing the workload

To make the workload ananymous there is two steps:
1. when creating the trace replace the username, account and jobname by user+UID, acct+GID and job+JOBID respectively and keep the association inside a file;
2. moreover, the file containing the dump of the slurm database should be updated to replace names by adding another parameter of `trace_builder_mysql`.

## Scalability of number of jobs

As mentioned in the paper, burst of jobs can make Slurm not responding and together with a fast clock it can make a experiment irrelevant.
One option will be to reduce the Slurm-Replay clock frequency when burst occurs, but there is the risk that the adaptive clock stays to low clock rate for a large part of the replay.
The best option is to investigate what is making Slurm not progressing fast enough and to boost its responsiveness.
The goal is to be able to reach sustainable experiemnts for a clock rate of 0.05 and lower.
