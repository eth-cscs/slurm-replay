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
#include "logger.h"
#include "shmemclock.h"


#define IS_NODE_IDLE(_X)		\
	((_X->node_state & NODE_STATE_BASE) == NODE_STATE_IDLE)

#define IS_NODE_DOWN(_X)		\
	((_X->node_state & NODE_STATE_BASE) == NODE_STATE_DOWN)

#define IS_NODE_DRAIN(_X)		\
	(_X->node_state & NODE_STATE_DRAIN)


char *workload_filename = NULL;
node_trace_t* node_arr_s;
node_trace_t* node_arr_e;
unsigned long long nnodes = 0;
double clock_rate = 0.0;

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

static char * node_state_string(node_info_t *node_ptr) {
    if (node_ptr) {
        if (IS_NODE_IDLE(node_ptr))
            return "IDLE";
        if (IS_NODE_DRAIN(node_ptr))
            return "DRAIN";
        if (IS_NODE_DOWN(node_ptr))
            return "DOWN";
    }
    return "UNKNOWN";
}

static int check_node_before_update(node_trace_t noded, int action)
{
    char *info;
    int to_update = 0;
    node_info_msg_t *nodeinfo;
    node_info_t *node_ptr = NULL;
    int res;

    // update node only on certain state and action
    res = slurm_load_node_single(&nodeinfo, noded.node_name, SHOW_ALL);
    node_ptr = &(nodeinfo->node_array[0]);
    if ( res != SLURM_SUCCESS) {
        log_error("slurm_load_node_single: %s for %s", slurm_strerror(slurm_get_errno()), noded.node_name);
    } else {
        to_update = node_ptr &&
                    ((action == NODE_STATE_DRAIN && !IS_NODE_DRAIN(node_ptr)) ||
                    (action == NODE_STATE_UNDRAIN && IS_NODE_DRAIN(node_ptr)));
        if (! to_update ) {
            info = "UNDRAINED";
            if (action == NODE_STATE_DRAIN)
                info = "DRAINED";
            log_info("wrong state: %s, to update %s to %s", node_state_string(node_ptr), noded.node_name, info);
        }
    }
    slurm_free_node_info_msg(nodeinfo);
    return to_update;
}

static void update_node_state(node_trace_t noded, int action)
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
        dmesg.node_state = NODE_STATE_DRAIN;
    }
    if (action == NODE_STATE_UNDRAIN) {
        info = "UNDRAINED";
        dmesg.node_state = NODE_STATE_UNDRAIN;
    }

    res = slurm_update_node(&dmesg);
    if ( res != SLURM_SUCCESS) {
        log_error("slurm_update_node: %s for %s count=%d", slurm_strerror(slurm_get_errno()), noded.node_name, count);
    } else {
        log_info("updated node: %s count=%d %s", noded.node_name,  count, info);
    }
    count++;

    if (dmesg.node_names) free(dmesg.node_names);
    if (dmesg.reason) free(dmesg.reason);
}

static void control_nodes()
{
    const int one_second = 1000000;
    int freq;
    time_t current_time = 0;
    unsigned long long ks = 0;
    unsigned long long ke = 0;

    current_time= get_shmemclock();
    freq = one_second*clock_rate;

    for(ks = 0; ks < nnodes; ks++) {
        if (node_arr_s[ks].preset) {
            update_node_state(node_arr_s[ks], NODE_STATE_DRAIN);
        } else {
            break;
        }
    }

    while( ks+ke < 2*nnodes ) {
        // wait for event either drain/maint node or resume node
        while((current_time < node_arr_s[ks].time_start || ks >=nnodes) && (current_time < node_arr_e[ke].time_end || ke >= nnodes)) {
            current_time = get_shmemclock();
            usleep(freq);
        }

        if (current_time >= node_arr_s[ks].time_start && ks < nnodes) {
            if (check_node_before_update(node_arr_s[ks], NODE_STATE_DRAIN))
                update_node_state(node_arr_s[ks], NODE_STATE_DRAIN);
            ks++;
        }
        if (current_time >= node_arr_e[ke].time_end && ke < nnodes) {
            if (check_node_before_update(node_arr_e[ke], NODE_STATE_UNDRAIN))
                update_node_state(node_arr_e[ke], NODE_STATE_UNDRAIN);
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

    log_info("total node state records: %lu, start time %ld", nnodes, node_arr_s[0].time_start);
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
    daemonize(daemon_flag,"log/node_controller.log");

    if (read_job_trace(workload_filename) < 0) {
        log_error("a problem was detected when reading trace file.");
        exit(-1);
    }

    //Jobs and reservations are submit when the replayed time clock equal their submission time
    control_nodes();
    log_info("node controler ends.");

    if (daemon_flag) fclose(logger);

    free(node_arr_s);
    free(node_arr_e);

    exit(0);

}
