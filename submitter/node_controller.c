#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <pwd.h>
#include <time.h>

#include <string.h>
#include <stdarg.h>
#include <slurm/slurm.h>

#include "trace.h"
#include "shmemclock.h"

FILE *logger = NULL;
char *workload_filename = NULL;
node_trace_t* node_arr_s;
node_trace_t* node_arr_e;
unsigned long long nnodes = 0;
static int daemon_flag = 1;
double clock_rate = 0.0;

static void log_string(const char* type, char* msg)
{
    char log_time[32];
    time_t t = get_shmemclock();
    struct tm timestamp_tm;

    localtime_r(&t, &timestamp_tm);
    strftime(log_time, 32, "%Y-%m-%dT%T", &timestamp_tm);
    fprintf(logger,"[%s.000] %s: %s\n", log_time, type, msg);
    fflush(logger);
}

static void log_error(char *fmt, ...)
{
    char dest[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(dest, fmt, argptr);
    va_end(argptr);
    log_string("error", dest);
}

static void log_info(char *fmt, ...)
{
    char dest[1024];
    va_list argptr;
    va_start(argptr, fmt);
    vsprintf(dest, fmt, argptr);
    va_end(argptr);
    log_string("info", dest);
}

int time_start_comp(const void *v1, const void *v2)
{
    const node_trace_t *p1 = (node_trace_t *)v1;
    const node_trace_t *p2 = (node_trace_t *)v2;
    if (p1->time_start < p2->time_start)
        return -1;
    else if (p1->time_start > p2->time_start)
        return 1;
    else
        return 0;
}

int time_end_comp(const void *v1, const void *v2)
{
    const node_trace_t *p1 = (node_trace_t *)v1;
    const node_trace_t *p2 = (node_trace_t *)v2;
    if (p1->time_end < p2->time_end)
        return -1;
    else if (p1->time_end > p2->time_end)
        return 1;
    else
        return 0;
}

static int update_node_state(node_trace_t noded, int action)
{
    static unsigned long count = 0;
    update_node_msg_t dmesg;
    char *info;
    int res;

    slurm_init_update_node_msg(&dmesg);

    dmesg.node_names = strdup(noded.node_name);
    dmesg.reason = strdup(noded.reason);

    if (action == NODE_STATE_DRAIN) {
        info = "DRAINED";
        dmesg.node_state = NODE_STATE_DRAIN;//noded.state;
    }
    if (action == NODE_RESUME) {
        info = "RESUMED";
        dmesg.node_state = NODE_RESUME;
    }

    res = slurm_update_node(&dmesg);
    if ( res != 0) {
        log_error("slurm_update_node: %s for %s count=%d", slurm_strerror(res), noded.node_name, count);
    } else {
        log_info("updated node: %s count=%d %s", noded.node_name,  count, info);
    }
    count++;

    if (dmesg.node_names) free(dmesg.node_names);
    if (dmesg.reason) free(dmesg.reason);
}

static void control_nodes()
{
    time_t current_time = 0;
    unsigned long long ks = 0;
    unsigned long long ke = 0;

    current_time= get_shmemclock();

    while( ks < nnodes && ke < nnodes ) {
        // wait for event either drain/maint node or resume node
        while(current_time < node_arr_s[ks].time_start && current_time < node_arr_e[ke].time_end ) {
            current_time= get_shmemclock();
            usleep(500);
        }

        if (current_time >= node_arr_s[ks].time_start && ks < nnodes) {
            //log_info("submitting %d job: time %lu | id %d", k, job_arr[k].time_submit, job_arr[k].id_job);
            update_node_state(node_arr_s[ks], NODE_STATE_DRAIN);
            ks++;
        }
        if (current_time >= node_arr_e[ke].time_end && ke < nnodes) {
            //log_info("submitting %d reservation: time %lu | name %s", k, resv_arr[k].time_start, resv_arr[k].resv_name);
            update_node_state(node_arr_e[ke], NODE_RESUME);
            ke++;
        }
    }
}

static int read_job_trace(const char* trace_file_name)
{
    unsigned long long njobs, nresvs;
    int trace_file;
    size_t query_length = 0;
    char query[1024];
    unsigned long k;
    long k_create, k_last, k_create_next;
    int k_id, k_id_next;

    trace_file = open(trace_file_name, O_RDONLY);
    if (trace_file < 0) {
        log_error("opening file %s\n", trace_file_name);
        return -1;
    }

    read(trace_file, &query_length, sizeof(size_t));
    lseek(trace_file, query_length*sizeof(char), SEEK_CUR);

    read(trace_file, &njobs, sizeof(unsigned long long));
    lseek(trace_file, sizeof(job_trace_t)*njobs, SEEK_CUR);

    read(trace_file, &nresvs, sizeof(unsigned long long));
    lseek(trace_file, sizeof(resv_trace_t)*nresvs, SEEK_CUR);

    read(trace_file, &nnodes, sizeof(unsigned long long));
    node_arr_s = (node_trace_t*)malloc(sizeof(node_trace_t)*nnodes);
    node_arr_e = (node_trace_t*)malloc(sizeof(node_trace_t)*nnodes);
    if (!node_arr_s) {
        log_error("unable to allocate memory for all node state records.\n");
        return -1;
    }
    read(trace_file, node_arr_s, sizeof(node_trace_t)*nnodes);
    memcpy(node_arr_e, node_arr_s, sizeof(node_trace_t)*nnodes);

    // sort by time_start and time_end
    qsort(node_arr_s, nnodes, sizeof(node_trace_t), time_start_comp);
    qsort(node_arr_e, nnodes, sizeof(node_trace_t), time_end_comp);

    log_info("total node state records: %lu, start time %lu", nnodes, node_arr_s[0].time_start);
    close(trace_file);

    return 0;
}

char   help_msg[] = "\
node_controller -w <workload_trace>\n\
      -w, --wrkldfile filename 'filename' is the name of the trace file \n\
                   containing the information of the jobs to \n\
                   simulate.\n\
      -D, --nodaemon do not daemonize the process\n\
      -r, --clockrate clock rate of the simulated clock\n\
      -h, --help           This help message.\n";



static void get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"wrkldfile", 1, 0, 'w'},
        {"nodaemon", 0, 0, 'D'},
        {"clockrate", 1, 0, 'r'},
        {"help", 0, 0, 'h'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "hw:Dr:", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('w'):
            workload_filename = strdup(optarg);
            break;
        case ('D'):
            daemon_flag = 0;
            break;
        case ('r'):
            clock_rate = strtod(optarg,NULL);
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


int main(int argc, char *argv[], char *envp[])
{
    //Open shared priority queue for time clock
    open_rdonly_shmemclock();

    get_args(argc, argv);

    if ( workload_filename == NULL) {
        printf("Usage: %s\n", help_msg);
        exit(-1);
    }

    // goes in daemon state
    daemonize(daemon_flag);

    if (read_job_trace(workload_filename) < 0) {
        log_error("a problem was detected when reading trace file.");
        exit(-1);
    }

    //Jobs and reservations are submit when the replayed time clock equal their submission time
    control_nodes();

    if (daemon_flag) fclose(logger);

    free(node_arr_s);
    free(node_arr_e);

    exit(0);

}
