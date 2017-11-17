#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <pwd.h>

#include <string.h>
#include <slurm/slurm.h>

#include "trace.h"
#include "shmemclock.h"

FILE *logger = NULL;
char *tfile = NULL;
char *username = NULL;
uid_t userid = 0;
uid_t groupid = 0;
char *workload_filename = NULL;
job_trace_t* job_arr;
resv_trace_t* resv_arr;
unsigned long long njobs = 0;
unsigned long long nresvs = 0;
static int daemon_flag = 1;
char *global_envp[100];

static void print_job_specs(job_desc_msg_t* dmesg)
{
    fprintf(logger, "\tdmesg->job_id: %d\n", dmesg->job_id);
    fprintf(logger, "\t\tdmesg->time_limit: %d\n", dmesg->time_limit);
    fprintf(logger, "\t\tdmesg->name: (%s)\n", dmesg->name);
    fprintf(logger, "\t\tdmesg->account: (%s)\n", dmesg->account);
    fprintf(logger, "\t\tdmesg->user_id: %d\n", dmesg->user_id);
    fprintf(logger, "\t\tdmesg->group_id: %d\n", dmesg->group_id);
    fprintf(logger, "\t\tdmesg->work_dir: (%s)\n", dmesg->work_dir);
    fprintf(logger, "\t\tdmesg->qos: (%s)\n", dmesg->qos);
    fprintf(logger, "\t\tdmesg->partition: (%s)\n", dmesg->partition);
    fprintf(logger, "\t\tdmesg->min_nodes: %d\n", dmesg->min_nodes);
    fprintf(logger, "\t\tdmesg->features: (%s)\n", dmesg->features);
    fprintf(logger, "\t\tdmesg->reservation: (%s)\n", dmesg->reservation);
//    fprintf(logger, "\t\tdmesg->dependency: (%s)\n", dmesg->dependency);
    fprintf(logger, "\t\tdmesg->env_size: %d\n", dmesg->env_size);
    fprintf(logger, "\t\tdmesg->environment[0]: (%s)\n", dmesg->environment[0]);
    fprintf(logger, "\t\tdmesg->script: (%s)\n", dmesg->script);
    fflush(logger);
}

static void create_script(char* script, int nodes, int tasks, long int jobid, long int duration, int exitcode)
{
    FILE* fp;
    char* line = NULL;
    char token[64];
    char val[64];
    size_t len = 0;
    size_t read, k, j, i = 0;

    fp = fopen(tfile,"r");
    if (fp == NULL) {
        fprintf(logger, "Cannot open template script file.");
        fflush(logger);
        exit(1);
    }

    while (( read = getline(&line, &len, fp)) != -1) {
        k = 0;
        while( k < read) {
            if (line[k] != '<') {
                script[i] = line[k];
                i++;
                k++;
            } else {
                // skip '<'
                k++;
                j = 0;
                while(line[k+j] != '>') {
                    token[j] = line[k+j];
                    j++;
                }
                token[j]='\0';
                // skip '>'
                k+=j+1;
                if(strcmp(token,"JOB_TASKS")==0) {
                    sprintf(val,"%d",tasks);
                }
                if(strcmp(token,"JOB_NODES")==0) {
                    sprintf(val,"%d",nodes);
                }
                if(strcmp(token,"DURATION")==0) {
                    sprintf(val,"%lu",duration);
                }
                if(strcmp(token,"JOB_ID")==0) {
                    sprintf(val,"%lu",jobid);
                }
                if(strcmp(token,"EXIT_CODE")==0) {
                    sprintf(val,"%d",exitcode);
                }
                for(j = 0; j < strlen(val); j++,i++) {
                    script[i] = val[j];
                }
            }
        }
    }
    script[i] = '\0';
    fclose(fp);
}


static void userids_from_name()
{
    struct passwd *pwd;
    uid_t u;
    char *endptr;

    if ( username == NULL || *username == '\0') {
        userid = geteuid();
        groupid = getegid();
    } else {
        userid = strtol(username, &endptr, 10); /* As a convenience to caller allow a numeric string */
        if (*endptr == '\0') {
            pwd = getpwuid(userid);
        } else {
            pwd = getpwnam(username);
        }
        if (pwd != NULL) {
            userid = pwd->pw_uid;
            groupid = pwd->pw_gid;
        } else {
            fprintf(logger,"Error: get uid and gid of user: %s\n", username);
            fflush(logger);
        }
    }
}


static int create_and_submit_job(job_trace_t jobd)
{
    job_desc_msg_t dmesg;
    submit_response_msg_t * respMsg = NULL;
    long int duration;
    int tasks;
    int rv = 0;
    char script[2048];
    int TRES_CPU = 1;
    int TRES_MEM = 2;
    int TRES_NODE = 4;

    slurm_init_job_desc_msg(&dmesg);

    dmesg.time_limit = jobd.timelimit;
    dmesg.job_id = jobd.id_job;
    dmesg.name = strdup(jobd.job_name);
    dmesg.account = strdup(jobd.account);
    dmesg.user_id = userid;
    dmesg.group_id = groupid;

    //TODO change work_dir
    dmesg.work_dir = strdup("/tmp");

    // Let's be conservative and consider only the normal qos, that is the case on most
    // of the job running on normal partition of Daint
    assert(strcmp(jobd.partition, "normal") == 0);
    dmesg.qos = strdup("normal");
    dmesg.partition = strdup(jobd.partition);

    dmesg.min_nodes = jobd.nodes_alloc;
    // check if string starts with "gpu:0" meaning using constriant mc
    if (strncmp("gpu:0", jobd.gres_alloc, 5)) {
        dmesg.features = strdup("mc");
    } else {
        dmesg.features = strdup("gpu");
    }

    dmesg.environment  = (char**)malloc(sizeof(char*)*2);
    dmesg.environment[0] = strdup("HOME=/home/maximem");
    dmesg.env_size = 1;

    dmesg.reservation   = strdup(jobd.resv_name);

    //TODO dependency
    //dmesg.dependency    = strdup(jobd.dependency);
    //dmesg.dependency    = NULL;

    duration = jobd.time_end - jobd.time_start;
    create_script(script, jobd.nodes_alloc, tasks, jobd.id_job, duration, jobd.exit_code);
    dmesg.script = strdup(script);

    //print_job_specs(&dmesg);

    if ( rv = slurm_submit_batch_job(&dmesg, &respMsg) ) {
        fprintf(logger,"Error: slurm_submit_batch_job: %s\n", slurm_strerror(rv));
        fflush(logger);
    }

    if (respMsg) {
        fprintf(logger, "Job submitted: error_code=%u job_id=%u\n",respMsg->error_code, respMsg->job_id);
        fflush(logger);
    }
    // Cleanup
    if (respMsg) slurm_free_submit_response_response_msg(respMsg);

    if (dmesg.name)        free(dmesg.name);
    if (dmesg.work_dir)    free(dmesg.work_dir);
    if (dmesg.qos)         free(dmesg.qos);
    if (dmesg.partition)   free(dmesg.partition);
    if (dmesg.account)     free(dmesg.account);
    if (dmesg.reservation) free(dmesg.reservation);
    if (dmesg.dependency)  free(dmesg.dependency);
    if (dmesg.script)      free(dmesg.script);
    free(dmesg.environment[0]);
    free(dmesg.environment);

    return rv;
}


static void submit_jobs()
{
    time_t current_time = 0;
    unsigned long long k = 0;

    current_time= get_shmemclock();

    while( k < njobs ) {
        // wait for submission time of next job
        while(current_time < job_arr[k].time_submit) {
            current_time= get_shmemclock();
            usleep(500);
        }

        fprintf(logger, "[%d] Submitting job: time %lu | id %d\n", k, job_arr[k].time_submit, job_arr[k].id_job);
        fflush(logger);
        create_and_submit_job(job_arr[k]);
        k++;
    }
}

static int create_and_submit_resv(resv_trace_t resvd)
{
    resv_desc_msg_t dmesg;
    char *output_name;
    int res;

    dmesg.accounts = strdup(resvd.accts);
    dmesg.end_time = resvd.time_end;
    dmesg.flags = resvd.flags;
    dmesg.name = strdup(resvd.resv_name);
    dmesg.node_list = strdup(resvd.nodelist);
    dmesg.partition = strdup("normal");
    dmesg.start_time = resvd.time_start;

    output_name = slurm_create_reservation(&dmesg);
    if (output_name == NULL) {
        res = slurm_update_reservation(&dmesg);
        if ( res != 0) {
            fprintf(logger,"Error: slurm_create_reservation and slurm_update_reservation: %s\n", slurm_strerror(res));
        } else {
            fprintf(logger, "Update reservation: %s\n",output_name);
        }
    } else {
        fprintf(logger, "Reservation created: %s\n",output_name);
    }
    fflush(logger);

    if (dmesg.accounts) free(dmesg.accounts);
    if (dmesg.name) free(dmesg.name);
    if (dmesg.node_list) free(dmesg.node_list);
    if (dmesg.partition) free(dmesg.partition);
    if (output_name) free(output_name);
}

// no need to call Slurm RPC, all reservations are set before the clock spins
static int create_and_submit_resv_norpc(resv_trace_t resvd)
{
    resv_desc_msg_t dmesg;
    char command[2048];
    char strtime_start[20];
    char strtime_end[20];
    int exec_result, child;
    char flags[1024];
    flags[0]='\0';

    strftime(strtime_start, sizeof(strtime_start), "%Y-%m-%d %H:%M:%S", localtime(&resvd.time_start));
    strftime(strtime_end, sizeof(strtime_end), "%Y-%m-%d %H:%M:%S", localtime(&resvd.time_end));

    if (resvd.flags & RESERVE_FLAG_MAINT) {
        strcat(flags,"MAINT,");
    }
    if (resvd.flags & RESERVE_FLAG_DAILY) {
        strcat(flags,"DAILY,");
    }
    if (resvd.flags & RESERVE_FLAG_WEEKLY) {
        strcat(flags,"WEEKLY,");
    }
    if (resvd.flags & RESERVE_FLAG_IGN_JOBS) {
        strcat(flags,"IGNORE_JOBS,");
    }
    if (resvd.flags & RESERVE_FLAG_ANY_NODES) {
        strcat(flags,"ANY_NODES,");
    }
    if (resvd.flags & RESERVE_FLAG_STATIC) {
        strcat(flags,"STATIC_ALLOC,");
    }
    if (resvd.flags & RESERVE_FLAG_PART_NODES) {
        strcat(flags,"PART_NODES,");
    }
    if (resvd.flags & RESERVE_FLAG_OVERLAP) {
        strcat(flags,"OVERLAP,");
    }
//    if (resvd.flags & RESERVE_FLAG_SPEC_NODES) {
//        strcat(flags,"SPEC_NODES,");
//    }
    if (resvd.flags & RESERVE_FLAG_FIRST_CORES) {
        strcat(flags,"FIRST_CORES,");
    }
    if (resvd.flags & RESERVE_FLAG_TIME_FLOAT) {
        strcat(flags,"TIME_FLOAT,");
    }
    if (resvd.flags & RESERVE_FLAG_REPLACE) {
        strcat(flags,"REPLACE,");
    }
    if (resvd.flags & RESERVE_FLAG_PURGE_COMP) {
        strcat(flags,"PURGE_COMP,");
    }
    if (strlen(flags) > 0) {
        flags[strlen(flags)-1]='\0';
    }

    sprintf(command, "scontrol create reservation='%s' "
            "starttime='%s' endtime='%s' nodes='%s' flags='%s' accounts='%s' partitionname='normal'",
            resvd.resv_name, strtime_start, strtime_end, resvd.nodelist, flags, resvd.accts);
    child = fork();
    if(child == 0) { /* the child */
        if(execve(command, NULL, global_envp) < 0) {
            fprintf(logger,"Error in execve commad %s\n", command);
            fflush(logger);
        }
    }

    waitpid(child, &exec_result, 0);
    if(exec_result == 0) {
        fprintf(logger,"Reservation %s created.\n", resvd.resv_name);
        fflush(logger);
        return 0;
    }

    fprintf(logger,"Error creating reservation %s.\n", resvd.resv_name);
    fflush(logger);

    return -1;
}

static void submit_reservations()
{
    unsigned long long k = 0;
    while( k < nresvs ) {
        fprintf(logger, "[%d] Submitting reservation: time %lu | name %s\n", k, resv_arr[k].time_start, resv_arr[k].resv_name);
        fflush(logger);
        create_and_submit_resv(resv_arr[k]);
        k++;
    }
}


static int read_job_trace(const char* trace_file_name)
{
    struct stat  stat_buf;
    int nrecs = 0, idx = 0;
    int trace_file;
    size_t query_length = 0;
    char query[1024];

    trace_file = open(trace_file_name, O_RDONLY);
    if (trace_file < 0) {
        printf("Error opening file %s\n", trace_file_name);
        return -1;
    }

    read(trace_file, &query_length, sizeof(size_t));
    read(trace_file, query, query_length*sizeof(char));

    read(trace_file, &njobs, sizeof(unsigned long long));

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*njobs);
    if (!job_arr) {
        printf("Error: unable to allocate memory for all job records.\n");
        return -1;
    }
    read(trace_file, job_arr, sizeof(job_trace_t)*njobs);
    fprintf(logger,"Total job records: %lu, start time %lu\n", njobs, job_arr[0].time_submit);
    fflush(logger);


    read(trace_file, &nresvs, sizeof(unsigned long long));
    resv_arr = (resv_trace_t*)malloc(sizeof(resv_trace_t)*nresvs);
    if (!resv_arr) {
        printf("Error: unable to allocate memory for all reservation records.\n");
        return -1;
    }
    read(trace_file, resv_arr, sizeof(resv_trace_t)*nresvs);
    fprintf(logger,"Total reservation records: %lu\n", nresvs);
    fflush(logger);

    close(trace_file);

    return 0;
}

char   help_msg[] = "\
submitter -w <workload_trace> -t <template_script>\n\
      -u, --user username  user that will be used to launch the jobs, should be the replay user\n\
      -w, --wrkldfile filename 'filename' is the name of the trace file \n\
                   containing the information of the jobs to \n\
                   simulate.\n\
      -t, --template filename containing the templatied script used by the jobs\n\
      -D, --nodaemon do not daemonize the process\n\
      -h, --help           This help message.\n";



static void get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"template", 1, 0, 't'},
        {"wrkldfile", 1, 0, 'w'},
        {"user", 1, 0, 'u'},
        {"nodaemon", 0, 0, 'D'},
        {"help", 0, 0, 'h'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "ht:w:u:D", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('t'):
            tfile = strdup(optarg);
            break;
        case ('w'):
            workload_filename = strdup(optarg);
            break;
        case ('u'):
            username = strdup(optarg);
            break;
        case ('D'):
            daemon_flag = 0;
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }

}

void daemonize(int daemon_flag)
{
    pid_t pid = 0;
    pid_t sid = 0;

    if (daemon_flag) {
        pid = fork();

        if  (pid < 0 ) {
            printf("Daemonizing failed. Exit.");
            exit(pid);
        }

        if (pid > 0 ) {
            // terminate parent process
            exit(0);
        }

        umask(0);
        sid = setsid();
        if (sid < 0) {
            printf("Daemonizing failed. Exit.");
            exit(sid);
        }
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        logger = fopen("submitter.log", "w+");
    } else {
        logger = stdout;
    }
}


int main(int argc, char *argv[], char *envp[])
{
    int i;

    get_args(argc, argv);

    i = 0;
    while(envp[i]) {
        global_envp[i] = envp[i];
        i++;
    }

    if ( workload_filename == NULL || tfile == NULL) {
        printf("Usage: %s\n", help_msg);
        exit(-1);
    }

    // goes in daemon state
    daemonize(daemon_flag);

    if (read_job_trace(workload_filename) < 0) {
        fprintf(logger, "Error: a problem was detected when reading trace file.");
        fflush(logger);
        exit(-1);
    }

    //Open shared priority queue for time clock
    open_rdonly_shmemclock();

    userids_from_name();

    // Reservation are all submitted at once
    submit_reservations();

    //Jobs are submit when the replayed time clock equal their submission time
    submit_jobs();

    if (daemon_flag) fclose(logger);

    free(job_arr);

    exit(0);

}
