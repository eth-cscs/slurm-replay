#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/types.h>
#include <pwd.h>

#include <string.h>
#include <slurm/slurm.h>

#include "trace.h"
#include "shmemclock.h"

FILE *logger = NULL;
char *workload_filename = NULL;
node_trace_t* node_arr;
unsigned long nodes = 0;
static int daemon_flag = 1;

/*static int
create_and_submit_job(job_trace_t jobd)
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

    //dmesg.priority = jobd.priority;

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

    //TODO there is no reservation field anymore
    //dmesg.reservation   = strdup(jobd.reservation);

    //TODO there is no dependency field
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
    unsigned long k = 0;

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

    fstat(trace_file, &stat_buf);
    nrecs = (stat_buf.st_size-sizeof(size_t)-query_length*sizeof(char)) / sizeof(job_trace_t);

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*nrecs);
    if (!job_arr) {
        printf("Error.  Unable to allocate memory for all job records.\n");
        return -1;
    }
    njobs = nrecs;

    while (read(trace_file, &job_arr[idx], sizeof(job_trace_t))) {
        ++idx;
    }

    fprintf(logger,"Trace initialization done. Total trace records: %lu. Start time %lu\n", njobs, job_arr[0].time_submit);
    fflush(logger);

    close(trace_file);

    return 0;
}
*/
char   help_msg[] = "\
submitter -w <workload_trace> -t <template_script>\n\
      -w, --wrkldfile filename 'filename' is the name of the trace file \n\
                   containing the information of the node availability \n\
      -D, --nodaemon do not daemonize the process\n\
      -h, --help           This help message.\n";



static void
get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"wrkldfile", 1, 0, 'w'},
        {"nodaemon", 0, 0, 'D'},
        {"help", 0, 0, 'h'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "hw:D", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
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

        logger = fopen("node_controller.log", "w+");
    } else {
        logger = stdout;
    }
}


int main(int argc, char *argv[])
{
    unsigned long k = 0;

    get_args(argc, argv);

    if ( workload_filename == NULL ) {
        printf("Usage: %s\n", help_msg);
        exit(-1);
    }

    // goes in daemon state
    daemonize(daemon_flag);

    /*    if (read_job_trace(workload_filename) < 0) {
            fprintf(logger, "Error: a problem was detected when reading trace file.");
            fflush(logger);
            exit(-1);
        }

        //Open shared priority queue for time clock
        open_rdonly_shmemclock();

        userids_from_name();

        //Jobs are submit when the replayed time clock equal their submission time
        submit_jobs();
    */
    if (daemon_flag) fclose(logger);

//    free(job_arr);

    exit(0);

}
