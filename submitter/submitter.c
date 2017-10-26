#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/types.h>

#include <string.h>
#include <slurm/slurm.h>

#include "trace.h"
#include "shmem_pq.h"

#define MAXUSER 5000

FILE *logger = NULL;
char *workload_filename = NULL;
char *ufile = NULL;
job_trace_t* job_arr;
unsigned long njobs = 0;
unsigned int nusers = 0;
char username[MAXUSER][100];
char account[MAXUSER][100];
static int daemon_flag = 1;

static void render_userfile()
{
    int t1,t2;
    unsigned int i = 0;
    char lines[100];
    FILE *up = fopen(ufile, "r");
    if (up == 0) {
        fprintf(logger, "failed to open %s\n", ufile);
        exit(1);
    } else  {
        while (i < MAXUSER && fgets(lines, sizeof(lines), up)) {
            t1 = strcspn (lines,":");
            t2 = strlen(lines);
            strncpy(username[i], lines, t1);
            username[i][t1] = '\0';
            strncpy(account[i], lines+t1+1, t2-1);
            strtok(account[i],"\n");
            i = i + 1;
        }
        nusers = i;
    }

}

static uid_t
userid_from_name(const char *name)//, gid_t* gid)
{
    int k;
    char *endptr;
    uid_t v;

    for(k = 0; k < nusers; k++) {
        if (strcmp(name, username[k]) == 0) {
            return strtol(account[k],&endptr,10);
        }
    }
    return 0;
}

static void
print_job_specs(job_desc_msg_t* dmesg)
{
    fprintf(logger, "\tdmesg->job_id: %d\n", dmesg->job_id);
    fprintf(logger, "\t\tdmesg->time_limit: %d\n", dmesg->time_limit);
    fprintf(logger, "\t\tdmesg->name: (%s)\n", dmesg->name);
    fprintf(logger, "\t\tdmesg->user_id: %d\n", dmesg->user_id);
    fprintf(logger, "\t\tdmesg->group_id: %d\n", dmesg->group_id);
    fprintf(logger, "\t\tdmesg->work_dir: (%s)\n", dmesg->work_dir);
    fprintf(logger, "\t\tdmesg->qos: (%s)\n", dmesg->qos);
    fprintf(logger, "\t\tdmesg->partition: (%s)\n", dmesg->partition);
    fprintf(logger, "\t\tdmesg->account: (%s)\n", dmesg->account);
    fprintf(logger, "\t\tdmesg->reservation: (%s)\n", dmesg->reservation);
//    fprintf(logger, "\t\tdmesg->dependency: (%s)\n", dmesg->dependency);
    fprintf(logger, "\t\tdmesg->num_tasks: %d\n", dmesg->num_tasks);
    fprintf(logger, "\t\tdmesg->min_cpus: %d\n", dmesg->min_cpus);
    fprintf(logger, "\t\tdmesg->cpus_per_task: %d\n", dmesg->cpus_per_task);
    fprintf(logger, "\t\tdmesg->ntasks_per_node: %d\n", dmesg->ntasks_per_node);
    //fprintf(logger, "\t\tdmesg->env_size: %d\n", dmesg->env_size);
    //fprintf(logger, "\t\tdmesg->environment[0]: (%s)\n", dmesg->environment[0]);
    fprintf(logger, "\t\tdmesg->script: (%s)\n", dmesg->script);
}

static void create_script(char* script, int tasks, long int jobid, long int duration)
{
    FILE* fp;
    char* line = NULL;
    char token[64];
    char val[64];
    size_t len = 0;
    size_t read, k, j, i = 0;

    fp = fopen("template.script","r");
    if (fp == NULL) {
        fprintf(logger, "Cannot open template script file.");
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
                if(strcmp(token,"DURATION")==0) {
                    sprintf(val,"%lu",duration);
                }
                if(strcmp(token,"JOB_ID")==0) {
                    sprintf(val,"%lu",jobid);
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

static int
create_and_submit_job(job_trace_t jobd)
{
    job_desc_msg_t dmesg;
    submit_response_msg_t * respMsg = NULL;
    int rv = 0;
    char script[2048];
    uid_t uidt;
    //gid_t gidt;


    slurm_init_job_desc_msg(&dmesg);

    /* Set up and call Slurm C-API for actual job submission. */
    dmesg.time_limit    = jobd.wclimit;
    dmesg.job_id        = jobd.job_id;
    dmesg.name      = strdup("runner_job");

    //TODO change uid and gid
    uidt = 22888;//userid_from_name(jobd.username);//, &gidt);
    dmesg.user_id       = uidt;
    dmesg.group_id      = 1001;
    //dmesg.group_id      = gidt;
    //TODO change work_dir
    dmesg.work_dir      = strdup("/tmp"); /* hardcoded to /tmp for now */

    dmesg.qos           = strdup(jobd.qosname);
    dmesg.partition     = strdup(jobd.partition);
    dmesg.account       = strdup(jobd.account);
    dmesg.reservation   = strdup(jobd.reservation);
    dmesg.dependency    = strdup(jobd.dependency);
//    dmesg.dependency    = NULL;
    dmesg.num_tasks     = jobd.tasks;
    dmesg.min_cpus      = jobd.tasks;
    dmesg.cpus_per_task = jobd.cpus_per_task;
    dmesg.ntasks_per_node = jobd.tasks_per_node;

    /* Need something for environment--Should make this more generic! */
    dmesg.environment  = (char**)malloc(sizeof(char*)*2);
    dmesg.environment[0] = strdup("HOME=/home/maximem");
    dmesg.env_size = 1;

    /* Standard script adding a time value in the clock and blocking until the clock reaches it */
    create_script(script, jobd.tasks, jobd.job_id, jobd.duration);
    dmesg.script        = strdup(script);

    print_job_specs(&dmesg);

    if ( slurm_submit_batch_job(&dmesg, &respMsg) ) {
        fprintf(logger,"Error: slurm_submit_batch_job\n");
        rv = -1;
    }

    if (respMsg)
        fprintf(logger, "Response from job submission\n\terror_code: %u"
                "\n\tjob_id: %u\n",
                respMsg->error_code, respMsg->job_id);
    // Cleanup
    if (respMsg) slurm_free_submit_response_response_msg(respMsg);

    //if (script) free(script);
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
    shmem_pq_elt_type current_time = 0;
    unsigned long k = 0;

    if (!empty_pq()) {
        current_time= top_pq();
    }

    // njobs time are in the queue, the queue cannot be empty
    while( k < njobs ) {
        while(current_time < job_arr[k].submit) {
            current_time = top_pq();
            usleep(500);
        }

        // block time progression and submit jobs
        if(current_time == job_arr[k].submit && k < njobs) {
//            lock_pq();
            fprintf(logger, "Submitting job: time %lu | id %d\n", job_arr[k].submit, job_arr[k].job_id);
            create_and_submit_job(job_arr[k]);
            // pop is done by slurm when the job is registered
            // this happens at the end of function _slurm_rpc_submit_batch_job
            // when this function completes the job information is in the slurm database
            usleep(1000);
//            unlock_pq();
            k++;
        }

    }
}

static int read_job_trace(const char* trace_file_name)
{
    struct stat  stat_buf;
    int nrecs = 0, idx = 0;
    int trace_file;

    trace_file = open(trace_file_name, O_RDONLY);
    if (trace_file < 0) {
        printf("Error opening file %s\n", trace_file_name);
        return -1;
    }

    fstat(trace_file, &stat_buf);
    nrecs = stat_buf.st_size / sizeof(job_trace_t);

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*nrecs);
    if (!job_arr) {
        printf("Error.  Unable to allocate memory for all job records.\n");
        return -1;
    }
    njobs = nrecs;

    while (read(trace_file, &job_arr[idx], sizeof(job_trace_t))) {
        ++idx;
    }

    printf("Trace initialization done. Total trace records: %lu. Start time %lu\n", njobs, job_arr[0].submit);

    close(trace_file);

    return 0;
}

char   help_msg[] = "\
submitter -w <workload_trace>\n\
      -w, --wrkldfile filename 'filename' is the name of the trace file \n\
                   containing the information of the jobs to \n\
                   simulate.\n\
      -u, --users filename containing the user names and accounts\n\
      -D, --nodaemon do not daemonize the process\n\
      -h, --help           This help message.\n";



static void
get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"users",   1, 0, 'u'},
        {"wrkldfile",   1, 0, 'w'},
        {"nodaemon",   0, 0, 'D'},
        {"help",    0, 0, 'h'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "hu:w:D", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('u'):
            ufile = strdup(optarg);
            break;
        case ('w'):
            workload_filename = strdup(optarg);
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
    }

    logger = fopen("submitter.log", "w+");
}


int main(int argc, char *argv[])
{
    unsigned long k = 0;

    get_args(argc, argv);

    if ( workload_filename == NULL || ufile == NULL) {
        printf("Usage: %s\n", help_msg);
        exit(-1);
    }

    render_userfile();

    if (read_job_trace(workload_filename) < 0) {
        printf("A problem was detected when reading trace file.\n"
               "Exiting...");
        return -1;
    }

    //Open shared priority queue for time clock
    open_pq();

    //Insert all submission times
    /*lock_pq();
    for(k = 0; k < njobs; k++) {
        push_pq(job_arr[k].submit);
    }
    unlock_pq();*/

    // goes in daemon state
    daemonize(daemon_flag);

    //Jobs are submit when the discretized time clock equal their submission time
    submit_jobs();

    //close_pq();

    if (logger != NULL) fclose(logger);

    free(job_arr);

    exit(0);

}
