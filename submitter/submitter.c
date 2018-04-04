#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <pwd.h>
#include <time.h>
#include <math.h>

#include <string.h>
#include <stdarg.h>
#include <slurm/slurm.h>

#include "trace.h"
#include "logger.h"
#include "shmemclock.h"
#define ONE_OVER_BILLION 1E-9

#define RESV_CREATE 3
#define RESV_UPDATE 4

char *tfile = NULL;
char *username = NULL;
uid_t userid = 0;
uid_t groupid = 0;
char *workload_filename = NULL;
job_trace_t* job_arr;
resv_trace_t* resv_arr;
unsigned long long njobs = 0;
unsigned long long nresvs = 0;
double clock_rate = 0.0;
int *resv_action;
int use_preset = 1;
int use_switch = 0;
double switch_rate = 1.0;
int switch_node = 0;
int switch_tick = 0;
int use_runtime = 0;
double var_timelimit = -1.0;

static void print_job_specs(job_desc_msg_t* dmesg)
{
    log_info("\tdmesg->job_id: %d", dmesg->job_id);
    log_info("\t\tdmesg->time_limit: %d", dmesg->time_limit);
    log_info("\t\tdmesg->name: (%s)", dmesg->name);
    log_info("\t\tdmesg->account: (%s)", dmesg->account);
    log_info("\t\tdmesg->user_id: %d", dmesg->user_id);
    log_info("\t\tdmesg->group_id: %d", dmesg->group_id);
    log_info("\t\tdmesg->work_dir: (%s)", dmesg->work_dir);
    log_info("\t\tdmesg->qos: (%s)", dmesg->qos);
    log_info("\t\tdmesg->partition: (%s)", dmesg->partition);
    log_info("\t\tdmesg->min_nodes: %d", dmesg->min_nodes);
    log_info("\t\tdmesg->features: (%s)", dmesg->features);
    log_info("\t\tdmesg->reservation: (%s)", dmesg->reservation);
    log_info("\t\tdmesg->dependency: (%s)", dmesg->dependency);
    log_info("\t\tdmesg->env_size: %d", dmesg->env_size);
    log_info("\t\tdmesg->environment[0]: (%s)", dmesg->environment[0]);
    log_info("\t\tdmesg->environment[1]: (%s)", dmesg->environment[1]);
    log_info("\t\tdmesg->script: (%s)", dmesg->script);
}

static void create_script(char* script, int nodes, int tasks, long int jobid, long int duration, int exitcode, int preset, time_t time_end)
{
    FILE* fp;
    char* line = NULL;
    char token[64];
    char val[64];
    size_t len = 0;
    size_t read, k, j, i = 0;
    struct timespec tv;
    double time_nsec;

    fp = fopen(tfile,"r");
    if (fp == NULL) {
        log_error("cannot open template script file.");
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
                if(strcmp(token,"CLOCK_RATE")==0) {
                    sprintf(val,"%f",clock_rate);
                }
                if(strcmp(token,"INIT_TIME")==0) {
                    clock_gettime(CLOCK_REALTIME, &tv);
                    time_nsec = tv.tv_sec + tv.tv_nsec * ONE_OVER_BILLION;
                    sprintf(val,"%f",time_nsec);
                }
                if(strcmp(token,"PRESET")==0) {
                    sprintf(val,"%d",preset);
                }
                if(strcmp(token,"TIME_END")==0) {
                    sprintf(val,"%ld",time_end);
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
            log_error("get uid and gid of user:%s",username);
        }
    }
}

static int create_and_submit_job(job_trace_t jobd)
{
    static unsigned long count = 0;
    job_desc_msg_t dmesg;
    submit_response_msg_t * respMsg = NULL;
    long int duration, org_duration;
    int tasks;
    int rv = 0;
    char script[2048];
    int TRES_CPU = 1;
    int TRES_MEM = 2;
    int TRES_NODE = 4;
    char env_str[256];
    int new_timelimit;
    int max_timelimit;
    int nswitch;

    slurm_init_job_desc_msg(&dmesg);

    dmesg.job_id = jobd.id_job;
    dmesg.name = strdup(jobd.job_name);
    dmesg.account = strdup(jobd.account);
    dmesg.user_id = userid;
    dmesg.group_id = groupid;

    sprintf(env_str,"/%s/tmp",username);
    dmesg.work_dir = strdup(env_str);

    dmesg.qos = strdup(jobd.qos_name);
    dmesg.partition = strdup(jobd.partition);
    if (strcmp("normal",jobd.partition) != 0 && \
        strcmp("xfer",jobd.partition) != 0 && \
        strcmp("large",jobd.partition) != 0 && \
        strcmp("debug",jobd.partition) != 0 && \
        strcmp("low",jobd.partition) != 0 && \
        strcmp("prepost",jobd.partition) != 0 && \
        strcmp("2go",jobd.partition) != 0) {
        log_info("job skipped: job_id=%u partition=%s count=%d", jobd.id_job, jobd.partition, count);
        return 0;
    }

    dmesg.min_nodes = jobd.nodes_alloc;
    // TODO: Daint specific, check if string starts with "gpu:0" meaning using constraint mc for all partitions except xfer,
    // one way to avoid that is to do this check at the trace creation
    // Note that sometime the string "gpu" is changed to the number "7696487" in the prodcution db.
    if (strncmp("gpu:0", jobd.gres_alloc, 5) == 0 || strncmp("7696487:0", jobd.gres_alloc,9) == 0) {
        if (strcmp(jobd.partition, "xfer") != 0) {
            dmesg.features = strdup("mc");
        }
    } else {
        if (strncmp("gpu:", jobd.gres_alloc, 4) == 0 || strncmp("7696487:", jobd.gres_alloc,8) == 0) {
            dmesg.features = strdup("gpu");
        }
    }

    dmesg.environment  = (char**)malloc(sizeof(char*)*2);
    sprintf(env_str,"HOME=/%s",username);
    dmesg.environment[0] = strdup(env_str);
    sprintf(env_str,"REPLAY_USER=%s",username);
    dmesg.environment[1] = strdup(env_str);
    dmesg.env_size = 2;

    if (use_preset != 3) {
        dmesg.reservation   = strdup(jobd.resv_name);
    }

    dmesg.dependency    = strdup(jobd.dependencies);
    org_duration = jobd.time_end - jobd.time_start;
    if (use_switch && jobd.nodes_alloc >= switch_node && org_duration >= switch_tick && (jobd.state == 3 || jobd.state == 6)) {
        nswitch = (int)ceil((double)(jobd.nodes_alloc)/384.0); // 48 blades per cabinet, 2 cabinets
        dmesg.req_switch = nswitch;
        duration = (int)ceil(org_duration*switch_rate);
    } else {
        nswitch = jobd.switches;
        dmesg.req_switch = jobd.switches;
        duration = org_duration;
    }

    if (use_preset == 0) {
        jobd.preset=0;
    } else if (use_preset == 2) {
        jobd.preset=1;
    } else if (use_preset == 3) {
        jobd.preset=1;
        dmesg.req_nodes = strdup(jobd.nodelist);
    }

    if (jobd.preset) {
        dmesg.priority = jobd.priority;
    }

    dmesg.time_limit = jobd.timelimit;
    if (use_runtime && var_timelimit != -1.0 && (jobd.state == 3 || jobd.state ==6)) { //state == complete
        new_timelimit = (int)ceil((var_timelimit*duration)/60.0);
        dmesg.time_limit = new_timelimit;
        max_timelimit = 24*60;
        if (new_timelimit > max_timelimit) {
            dmesg.time_limit = max_timelimit;
        }
    }
    create_script(script, jobd.nodes_alloc, tasks, jobd.id_job, duration, jobd.exit_code, jobd.preset, jobd.time_end);
    dmesg.script = strdup(script);
    rv = slurm_submit_batch_job(&dmesg, &respMsg);
    if (rv != SLURM_SUCCESS ) {
        log_error("%d slurm_submit_batch_job: %s [submit number=%d]", dmesg.job_id, slurm_strerror(slurm_get_errno()), count);
        print_job_specs(&dmesg);
    }

    if (respMsg) {
        log_info("job submitted [%d,%ld,%d]: job_id=%u count=%d switch=%d|%.1f,%d,%d timelimit=%d|%d duration=%d|%d preset=%d", jobd.nodes_alloc, org_duration, jobd.state, respMsg->job_id, count, nswitch, switch_rate, switch_node, switch_tick,  dmesg.time_limit, jobd.timelimit, duration, org_duration, jobd.preset);
    }
    count++;

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
    if (dmesg.req_nodes)      free(dmesg.req_nodes);
    free(dmesg.environment[0]);
    free(dmesg.environment[1]);
    free(dmesg.environment);

    return rv;
}

static void create_and_submit_resv(resv_trace_t resvd, int action)
{
    static unsigned long count = 0;
    resv_desc_msg_t dmesg;
    char *output_name = NULL;
    int res;

    slurm_init_resv_desc_msg(&dmesg);

    dmesg.accounts = strdup(resvd.accts);
    dmesg.end_time = resvd.time_end;
    dmesg.flags = resvd.flags;
    dmesg.name = strdup(resvd.resv_name);
    dmesg.node_list = strdup(resvd.nodelist);
    dmesg.partition = strdup("normal");

    if (action == RESV_CREATE) {
        dmesg.start_time = resvd.time_start;
        output_name = slurm_create_reservation(&dmesg);
        if (output_name == NULL) {
            log_error("%d slurm_create_reservation: %s %s [submit number=%d]", resvd.id_resv, resvd.resv_name, slurm_strerror(slurm_get_errno()), count);
        } else {
            log_info("reservation created: %s count=%d",output_name, count);
        }
    }
    if (action == RESV_UPDATE) {
        // do not update time_emd and time_start
        res = slurm_update_reservation(&dmesg);
        if ( res != 0) {
            log_error("%d slurm_update_reservation: %s %s count=%d", resvd.id_resv, resvd.resv_name, slurm_strerror(res), count);
        } else {
            log_info("updated reservation: %s count=%d",resvd.resv_name, count);
        }
    }
    count++;

    if (dmesg.accounts) free(dmesg.accounts);
    if (dmesg.name) free(dmesg.name);
    if (dmesg.node_list) free(dmesg.node_list);
    if (dmesg.partition) free(dmesg.partition);
    if (output_name) free(output_name);
}

static void create_and_submit_reservations(unsigned long long *npreset_resv)
{
    unsigned long long kr = 0;

    if (use_preset > 0) {
        for(kr = 0; kr < nresvs; kr++) {
            if (resv_action[kr] == RESV_CREATE) {
                create_and_submit_resv(resv_arr[kr], resv_action[kr]);
            } else {
                break;
            }
        }
    }
    *npreset_resv=kr;
}

static void submit_jobs_and_reservations(unsigned long long npreset_resv)
{
    const int one_second = 1000000;
    int freq;
    time_t current_time = 0;
    unsigned long long kj = 0, kr;
    kr = npreset_resv;

    current_time = get_shmemclock();

    freq = one_second*clock_rate;
    while( kj < njobs || kr < nresvs ) {
        // wait for submission time of next job or reservation
        while((current_time < job_arr[kj].time_submit || kj >= njobs) && (current_time < resv_arr[kr].time_start || kr >= nresvs)) {
            current_time = get_shmemclock();
            usleep(freq);
        }
        while (current_time >= resv_arr[kr].time_start && kr < nresvs) {
            create_and_submit_resv(resv_arr[kr], resv_action[kr]);
            kr++;
        }
        while (current_time >= job_arr[kj].time_submit && kj < njobs) {
            create_and_submit_job(job_arr[kj]);
            kj++;
        }
    }
}



static int read_job_trace(const char* trace_filename)
{
    int trace_file;
    size_t query_length = 0;
    char query[1024];
    unsigned long k, j;
    long k_create, k_last, k_create_next;
    int k_id, k_id_next;
    unsigned long long time_njobs;

    trace_file = open(trace_filename, O_RDONLY);
    if (trace_file < 0) {
        log_error("opening file %s\n", trace_filename);
        return -1;
    }

    read(trace_file, &query_length, sizeof(size_t));
    read(trace_file, query, query_length*sizeof(char));

    read(trace_file, &njobs, sizeof(unsigned long long));

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*njobs);
    if (!job_arr) {
        log_error("unable to allocate memory for all job records.\n");
        return -1;
    }
    read(trace_file, job_arr, sizeof(job_trace_t)*njobs);

    read(trace_file, &nresvs, sizeof(unsigned long long));
    resv_arr = (resv_trace_t*)malloc(sizeof(resv_trace_t)*nresvs);
    if (!resv_arr) {
        log_error("unable to allocate memory for all reservation records.\n");
        return -1;
    }
    read(trace_file, resv_arr, sizeof(resv_trace_t)*nresvs);

    close(trace_file);

    //prepare reservation separate create from update
    resv_action = malloc(nresvs*sizeof(int));

    //get all different reservation types: create or update and set time_end
    if (nresvs > 0 ) {
        for(k = 0; k < nresvs; k++) {
            //log_info("[%d] id=%d st=%d et=%d",k, resv_arr[k].id_resv, resv_arr[k].time_start, resv_arr[k].time_end);
            resv_action[k]=0;
        }
        k_create = 0;
        k_last = -1;
        k_id = resv_arr[k_create].id_resv;
        k_id_next = -1;
        while(k_id != -1) {
            //log_info("k_id=%d",k_id);
            for(k = 0; k < nresvs; k++) {
                if (resv_arr[k].id_resv == k_id) {
                    k_last = k;
                    //log_info("[%d] k_last=%d",k, k_last);
                    if (k == k_create) {
                        //log_info("[%d] k_create=%d",k, k_create);
                        resv_action[k] = RESV_CREATE;
                    } else {
                        //log_info("[%d] k_update=%d", k, k);
                        resv_action[k] = RESV_UPDATE;
                    }
                } else {
                    if (k_id_next == -1 && resv_action[k] == 0) {
                        k_id_next = resv_arr[k].id_resv;
                        k_create_next = k;
                        //log_info("[%d] k_id_next=%d k_create_next=%d", k, k_id_next, k_create_next);
                    }
                }
            }
            if (k_create != -1 && k_last != -1 && k_create != k_last) {
                for(k = 0; k < nresvs; k++) {
                    if (resv_arr[k].id_resv == resv_arr[k_last].id_resv) {
                        resv_arr[k].time_end = resv_arr[k_last].time_end;
                        //log_info("Change reservation %d time_end (%d, %d)", resv_arr[k].id_resv,  resv_arr[k].time_start, resv_arr[k].time_end);
                    }
                }
            }
            k_id = k_id_next;
            k_id_next = -1;
            k_last = -1;
            k_create = k_create_next;
        }
        /*for(k = 0; k < nresvs; k++) {
            if (resv_action[k] ==  RESV_CREATE)
                log_info("[%d] id=%d CR  st=%d et=%d",k, resv_arr[k].id_resv, resv_arr[k].time_start, resv_arr[k].time_end);
            else
                log_info("[%d] id=%d UP",k, resv_arr[k].id_resv);
        }*/

    }
    return 0;
}

char   help_msg[] = "\
submitter\n\
      -w <workload_trace> -t <template_script>\n\
      -u, --user username        user that will be used to launch the jobs, should be the replay user\n\
      -w, --wrkldfile filename   file is the name of the trace file \n\
      -t, --template filename    file containing the templatied script used by the jobs\n\
      -D, --nodaemon             do not daemonize the process\n\
      -r, --clockrate            clock rate of the Replay-clock\n\
      -p, --preset               use preset: 0 = none, 1 = job started before trace start date (default), 2 = all jobs have a set priority, 3 = like 2 plus fix hostlist and no reservation\n\
      -c, --timelimit            add a variation of time limit specified by the job. A value of 1.05 will add a 1.05x the real duration as a time limit\n\
      -x, --use_switch           x,y,z will use switch=1 for jobs using y nodes and z time in seconds, will apply the factor x to their duration\n\
      -h, --help                 This help message.\n";



static void get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"template", 1, 0, 't'},
        {"wrkldfile", 1, 0, 'w'},
        {"user", 1, 0, 'u'},
        {"nodaemon", 0, 0, 'D'},
        {"use_switch", 1, 0, 'x'},
        {"clockrate", 1, 0, 'r'},
        {"timelimit", 1, 0, 'c'},
        {"preset", 1, 0, 'p'},
        {"help", 0, 0, 'h'}
    };
    int opt_char, option_index, i, k;
    char *switch_v;
    char node_v[128];
    char rate_v[32];
    char tick_v[32];

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "c:ht:w:u:Dr:p:x:", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('t'):
            tfile = strdup(optarg);
            break;
        case ('p'):
            use_preset = atoi(optarg);
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
        case ('r'):
            clock_rate = strtod(optarg,NULL);
            break;
        case ('c'):
            use_runtime = 1;
            var_timelimit = strtod(optarg,NULL);
            break;
        case ('x'):
            use_switch = 1;
            switch_v = strdup(optarg);
            i = 0;
            while(switch_v[i] != ',' && switch_v[i] != '\0') {
                if (switch_v[i] != ' ') {
                    rate_v[i] = switch_v[i];
                }
                i++;
            }
            rate_v[i] = '\0';
            switch_rate = strtod(rate_v,NULL);
            if (switch_v[i] == ',') {
                i++;
                k = 0;
                while(switch_v[i] != ',' && switch_v[i] != '\0') {
                    node_v[k] = switch_v[i];
                    k++;
                    i++;
                }
                node_v[k] = '\0';
                switch_node = atoi(node_v);
            }
            if (switch_v[i] == ',') {
                i++;
                k = 0;
                while(switch_v[i] != '\0') {
                    tick_v[k] = switch_v[i];
                    k++;
                    i++;
                }
                tick_v[k] = '\0';
                switch_tick = atoi(tick_v);
            }
            free(switch_v);
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }

}

int main(int argc, char *argv[])
{
    unsigned long long npreset_resv = 0;

    //Open shared priority queue for time clock
    open_rdonly_shmemclock();

    get_args(argc, argv);

    if ( workload_filename == NULL || tfile == NULL) {
        printf("Usage: %s\n", help_msg);
        exit(-1);
    }

    if (read_job_trace(workload_filename) < 0) {
        log_error("a problem was detected when reading trace file.");
        exit(-1);
    }

    logger = fopen("log/submitter.log", "w+");

    userids_from_name();

    if (use_preset == 3) {
        // disable reservation
        nresvs=0;
    }

    log_info("total job records: %llu, start time %ld", njobs, job_arr[0].time_submit);
    log_info("total reservation records: %llu", nresvs);

    //Jobs and reservations are submit when the replayed time clock equal their submission time
    create_and_submit_reservations(&npreset_resv);
    log_info("Number of pre-created reservations: %llu\n", npreset_resv);

    // goes in daemon state
    daemonize(daemon_flag, "log/submitter.log");

    submit_jobs_and_reservations(npreset_resv);
    log_info("submitter ends.");

    fclose(logger);

    free(job_arr);
    free(resv_arr);
    free(resv_action);

    exit(0);

}
