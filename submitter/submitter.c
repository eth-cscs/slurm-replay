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
#define ONE_OVER_BILLION 1E-9

#define RESV_CREATE 3
#define RESV_UPDATE 4

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
double clock_rate = 0.0;
int *resv_action;

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
//    log_info("\t\tdmesg->dependency: (%s)", dmesg->dependency);
    log_info("\t\tdmesg->env_size: %d", dmesg->env_size);
    log_info("\t\tdmesg->environment[0]: (%s)", dmesg->environment[0]);
    log_info("\t\tdmesg->script: (%s)", dmesg->script);
}

static void create_script(char* script, int nodes, int tasks, long int jobid, long int duration, int exitcode)
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
        log_error("%d slurm_submit_batch_job: %s count=%d", dmesg.job_id, slurm_strerror(rv), count);
    }

    if (respMsg) {
        log_info("job submitted: job_id=%u count=%d", respMsg->job_id, count);
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
    free(dmesg.environment[0]);
    free(dmesg.environment);

    return rv;
}


static int create_and_submit_resv(resv_trace_t resvd, int action)
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
            log_error("%d slurm_create_reservation count=%d", resvd.id_resv, count);
        } else {
            log_info("reservation created: %s count=%d",output_name, count);
        }
    }
    if (action == RESV_UPDATE) {
        // do not update time_emd and time_start
        res = slurm_update_reservation(&dmesg);
        if ( res != 0) {
            log_error("%d slurm_update_reservation: %s count=%d", resvd.id_resv, slurm_strerror(res), count);
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

static void submit_jobs_and_reservations()
{
    time_t current_time = 0;
    unsigned long long kj = 0, kr = 0;

    current_time= get_shmemclock();

    while( kj < njobs || kr < nresvs ) {
        // wait for submission time of next job
        while(current_time < job_arr[kj].time_submit && current_time < resv_arr[kr].time_start ) {
            current_time= get_shmemclock();
            usleep(500);
        }

        if (current_time >= job_arr[kj].time_submit && kj < njobs) {
            //log_info("submitting %d job: time %lu | id %d", k, job_arr[k].time_submit, job_arr[k].id_job);
            create_and_submit_job(job_arr[kj]);
            kj++;
        }
        if (current_time >= resv_arr[kr].time_start && kr < nresvs) {
            //log_info("submitting %d reservation: time %lu | name %s", k, resv_arr[k].time_start, resv_arr[k].resv_name);
            create_and_submit_resv(resv_arr[kr], resv_action[kr]);
            kr++;
        }
    }
}



static int read_job_trace(const char* trace_file_name)
{
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
    read(trace_file, query, query_length*sizeof(char));

    read(trace_file, &njobs, sizeof(unsigned long long));

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*njobs);
    if (!job_arr) {
        log_error("unable to allocate memory for all job records.\n");
        return -1;
    }
    read(trace_file, job_arr, sizeof(job_trace_t)*njobs);
    log_info("total job records: %lu, start time %lu", njobs, job_arr[0].time_submit);


    read(trace_file, &nresvs, sizeof(unsigned long long));
    resv_arr = (resv_trace_t*)malloc(sizeof(resv_trace_t)*nresvs);
    if (!resv_arr) {
        log_error("unable to allocate memory for all reservation records.\n");
        return -1;
    }
    read(trace_file, resv_arr, sizeof(resv_trace_t)*nresvs);
    log_info("total reservation records: %lu", nresvs);

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
//                       log_info("Change reservation %d time_end (%d, %d)", resv_arr[k].id_resv,  resv_arr[k].time_start, resv_arr[k].time_end);
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
submitter -w <workload_trace> -t <template_script>\n\
      -u, --user username  user that will be used to launch the jobs, should be the replay user\n\
      -w, --wrkldfile filename 'filename' is the name of the trace file \n\
                   containing the information of the jobs to \n\
                   simulate.\n\
      -t, --template filename containing the templatied script used by the jobs\n\
      -D, --nodaemon do not daemonize the process\n\
      -r, --clockrate clock rate of the simulated clock\n\
      -h, --help           This help message.\n";



static void get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"template", 1, 0, 't'},
        {"wrkldfile", 1, 0, 'w'},
        {"user", 1, 0, 'u'},
        {"nodaemon", 0, 0, 'D'},
        {"clockrate", 1, 0, 'r'},
        {"help", 0, 0, 'h'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "ht:w:u:Dr:", long_options, &option_index)) == -1 )
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

        logger = fopen("submitter.log", "w+");
    } else {
        logger = stdout;
    }
}


int main(int argc, char *argv[], char *envp[])
{
    int i;

    //Open shared priority queue for time clock
    open_rdonly_shmemclock();

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
        log_error("a problem was detected when reading trace file.");
        exit(-1);
    }

    userids_from_name();

    //Jobs and reservations are submit when the replayed time clock equal their submission time
    submit_jobs_and_reservations();

    if (daemon_flag) fclose(logger);

    free(job_arr);
    free(resv_arr);
    free(resv_action);

    exit(0);

}
