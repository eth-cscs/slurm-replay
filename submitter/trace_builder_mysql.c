#include <stdio.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <mysql/mysql.h>
#else
#include <mariadb/mysql.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#define __USE_XOPEN
#include <time.h>

#include "trace.h"

typedef struct dependency {
    int job_ids;
    int switches;
    char *dependencies;
} deps_t;

char *endtime = NULL;
char *filename = NULL;
char *host = NULL;
char *dbname = NULL;
char *starttime = NULL;
char *cluster = NULL;
char job_table[256];
char resv_table[256];
char assoc_table[256];
char event_table[256];
char *user = NULL;
char partitions[2048] = "";
char *password;
static int use_where = 1;
static int do_resv = 1;
static int do_event = 1;
static int use_dependencies = 0;
char *dep_filename = NULL;
deps_t *depend;
static int nopreset = 0;
int port;

int cmpfunc(const void * a, const void * b)
{
    return (int)( ((deps_t*)a)->job_ids - ((deps_t*)b)->job_ids );
}

static void lookfor(unsigned long jid, unsigned long long n, char *dependencies, int *switches)
{
    deps_t *item = NULL;
    item = (deps_t*) bsearch (&jid, depend, n, sizeof(deps_t), cmpfunc);
    if ( item ) {
        if (item->dependencies) {
            strcpy(dependencies, item->dependencies);
        } else {
            dependencies[0] = '\0';
        }
        *switches = item->switches;
    } else {
        dependencies[0] = '\0';
        *switches = 0;
    }
}

static unsigned long long count_dependencies_jobs(const char* filename)
{
    FILE *fp;
    unsigned long long lines = 0;
    char ch;

    fp = fopen(filename,"r");
    if (fp == NULL) {
        printf( "Cannot open template script file.");
        exit(1);
    }

    while(!feof(fp)) {
        ch = fgetc(fp);
        if(ch == '\n') {
            lines++;
        }
    }
    fclose(fp);
    return lines;
}

static void load_dependency(const char* filename, unsigned long long n)
{
    FILE *fp;
    char* line = NULL;
    char token[2048];
    size_t read, len, dep_len, ni, k, i = 0;

    depend = malloc(n*sizeof(deps_t));

    fp = fopen(filename,"r");
    if (fp == NULL) {
        printf( "Cannot open template script file.");
        exit(1);
    }

    ni = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        i = 0;
        k = 0;
        while( line[i] != '|') {
            token[k] = line[i];
            k++;
            i++;
        }
        i++; //skip |
        token[k] = '\0';
        depend[ni].job_ids = atoi(token);
        k = 0;
        while( line[i] != '|') {
            token[k] = line[i];
            k++;
            i++;
        }
        i++; //skip |
        token[k] = '\0';
        dep_len = strlen(token);
        if (dep_len > 0) {
            depend[ni].dependencies = malloc(dep_len*sizeof(char));
            strcpy(depend[ni].dependencies,token);
        } else {
            depend[ni].dependencies=NULL;
        }
        k = 0;
        while( line[i] != '|') {
            token[k] = line[i];
            k++;
            i++;
        }
        token[k] = '\0';
        if (strlen(token) > 0) {
            depend[ni].switches = atoi(token);
        } else {
            depend[ni].switches = 0;
        }
        ni++;
    }
    fclose(fp);
}

void finish_with_error(MYSQL *con)
{
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    exit(1);
}

void print_usage()
{
    printf("\
Usage: mysql_trace_builder [OPTIONS]\n\
    -s, --starttime  time            Start selecting jobs from this time\n\
                                     format: \"yyyy-MM-DD hh:mm:ss\"\n\
    -e, --endtime    time            Stop selecting jobs at this time\n\
                                     format: \"yyyy-MM-DD hh:mm:ss\"\n\
    -d, --dbname     db_name         Name of the database\n\
    -h, --host       db_hostname     Name of machine hosting MySQL DB\n\
    -p, --password   password        Password to connect to the db\n\
    -P, --port       port            Port number of the machine hosting MySQL DB\n\
    -u, --user       dbuser          Name of user with which to establish a\n\
                                     connection to the DB\n\
    -c, --cluster    cluster_name     Name of the cluster used by the Slurm database to extract data\n\
    -a, --partition partition    a partition to include in the trace, this option should be repeated if more than one partition should be considered\n\
    -f, --file       filename        Name of the output trace file being created\n\
    -x, --dependencies filename      Name of the file containing the dependencies\n\
    -w, --where                      Do not use the where statement for the  SQL query to retrieve the data\n\
    -n, --noprest                    Do not preset the jobs\n\
    -?, --help                       This help message\n\n\
");
}

static void
get_args(int argc, char** argv)
{
    static struct option long_options[] = {
        {"endtime", required_argument, 0, 'e'},
        {"file", required_argument, 0, 'f'},
        {"host", required_argument, 0, 'h'},
        {"dbname", required_argument, 0, 'd'},
        {"help", no_argument,   0, '?'},
        {"password", required_argument,   0, 'p'},
        {"port", required_argument,   0, 'P'},
        {"where", no_argument,   0, 'w'},
        {"nopreset", no_argument,   0, 'n'},
        {"starttime", required_argument, 0, 's'},
        {"cluster", required_argument, 0, 'c'},
        {"dependencies", required_argument, 0, 'x'},
        {"user", required_argument, 0, 'u'},
        {"partition", required_argument, 0, 'a'},
        {0, 0, 0, 0}
    };
    int opt_char, option_index;
    char tmp[256];

    while(1) {
        if ((opt_char = getopt_long(argc, argv, "s:e:d:h:p:P:u:c:a:f:x:wn?", long_options, &option_index)) == -1 )
            break;
        switch  (opt_char) {
        case ('p'):
            password = strdup(optarg);
            break;
        case ('w'):
            use_where = 0;
            do_resv = 0;
            do_event = 0;
            break;
        case ('e'):
            endtime = optarg;
            break;
        case ('f'):
            filename = optarg;
            break;
        case ('P'):
            port = atoi(optarg);
            break;
        case ('h'):
            host = optarg;
            break;
        case ('n'):
            nopreset = 1;
            break;
        case ('d'):
            dbname = optarg;
            break;
        case ('?'):
            print_usage();
            exit(0);
        case ('s'):
            starttime = optarg;
            break;
        case ('c'):
            cluster = optarg;
            sprintf(job_table,"%s_job_table",cluster);
            sprintf(resv_table,"%s_resv_table",cluster);
            sprintf(assoc_table,"%s_assoc_table",cluster);
            sprintf(event_table,"%s_event_table",cluster);
            break;
        case ('u'):
            user = optarg;
            break;
        case ('a'):
            if (strlen(partitions) == 0) {
                sprintf(tmp,"'%s'", optarg);
                strcpy(partitions,tmp);
            } else {
                sprintf(tmp,",'%s'", optarg);
                strcat(partitions,tmp);
            }
            break;
        case ('x'):
            dep_filename = strdup(optarg);
            use_dependencies = 1;
            break;
        };
    }
}

/* This function should be changed depending on the Slurm configuration and data stored in the database. For instance, on Piz Daint some constraint are not clearly stored in the Slurm database and should be parsed */
const char* reconstruct_constraint(const char* gres) {
   if (strncmp("gpu:0", gres, 5) == 0 || strncmp("7696487:0", gres, 9) == 0) {
        return "mc";
   } else {
        if (strncmp("gpu:", gres, 4) == 0 || strncmp("7696487:", gres, 8) == 0) {
             return "gpu";
        }
   }
   return gres;
}

int main(int argc, char **argv)
{

    int c,written;
    int trace_file;
    int group_file;
    int passwd_file;
    char group_item[256];
    char user_item[256];
    char group_filename[256];
    char passwd_filename[256];
    struct tm tmVar;
    time_t time_start;
    time_t time_end;
    int CET = -3600;

    char query[1024];
    char where_statement[256];
    size_t query_length;
    //unsigned int num_fields;
    unsigned long long num_rows, npreset_jobs;
    unsigned long long w_num_rows, k;
    unsigned long long count_dep;

    MYSQL *conn;
    MYSQL_RES *result_job;
    MYSQL_RES *result_resv;
    MYSQL_RES *result_node;
    MYSQL_ROW row;
    job_trace_t job_trace;
    resv_trace_t resv_trace;
    node_trace_t fix_node_trace;
    off_t cur_offset;

    get_args(argc, argv);

    if ((user == NULL) || (host == NULL) || (dbname == NULL) || (filename == NULL) || (cluster == NULL)) {
        printf("user, host, dbname, cluster and trace file name cannot be NULL!\n");
        print_usage();
        exit(-1);
    }

    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, host, user, password, dbname, port, NULL, 0)) {
        finish_with_error(conn);
    }

    if ((starttime == NULL) || (cluster == NULL)) {
        printf("starttime and cluster cannot be NULL!\n");
        print_usage();
        exit(-1);
    }

    memset(&tmVar, 0, sizeof(struct tm));
    if ( strptime(starttime, "%Y-%m-%d %H:%M:%S", &tmVar) != NULL ) {
        time_start = mktime(&tmVar)+CET;
    } else {
        printf("ERROR: starttime not valid.\n");
        print_usage();
        exit(-1);
    }

    memset(where_statement,'\0',256);
    if (use_where) {
        /*validate input parameter to build the query*/
        if (endtime == NULL) {
            printf("endtime cannot be NULL!\n");
            print_usage();
            exit(-1);
        }

        memset(&tmVar, 0, sizeof(struct tm));
        if ( strptime(endtime, "%Y-%m-%d %H:%M:%S", &tmVar) != NULL ) {
            time_end = mktime(&tmVar)+CET;
        } else {
            printf("ERROR: endtime not valid.\n");
            print_usage();
            exit(-1);
        }

        sprintf(where_statement,
                "WHERE t.time_submit < %lu AND t.time_end > %lu AND t.time_start < %lu AND "
                "t.state <> 7 AND "
                "t.nodes_alloc > 0 AND t.partition IN (%s)",
                time_end, time_start, time_end, partitions);
    }
    memset(query,'\0',1024);
    sprintf(query, "SELECT t.account, t.exit_code, t.job_name, "
            "t.id_job, q.name, t.id_user, t.id_group, r.resv_name, t.nodelist, t.nodes_alloc, t.partition, t.state, "
            "t.timelimit, t.time_submit, t.time_eligible, t.time_start, t.time_end, t.time_suspended, "
            "t.gres_alloc, t.priority, a.user "
            "FROM %s as t LEFT JOIN %s as r ON t.id_resv = r.id_resv AND t.time_start >= r.time_start and t.time_end <= r.time_end "
            "LEFT JOIN qos_table as q ON q.id = t.id_qos "
            "LEFT JOIN %s as a ON t.id_assoc = a.id_assoc %s",
            job_table, resv_table, assoc_table, where_statement);
    printf("\nQuery --> %s\n\n", query);

    sprintf(group_filename, "%s_group", filename);
    sprintf(passwd_filename, "%s_passwd", filename);

    if (mysql_query(conn, query)) {
        finish_with_error(conn);
    }
    result_job = mysql_store_result(conn);

    if (result_job == NULL) {
        finish_with_error(conn);
    }

    //num_fields = mysql_num_fields(result_job);
    num_rows = mysql_num_rows(result_job);

    /* writing results to file */
    if((trace_file = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR |
                          S_IRGRP | S_IROTH)) < 0) {
        printf("Error opening file %s\n", filename);
        exit(-1);
    }
    if((group_file = open(group_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR |
                          S_IRGRP | S_IROTH)) < 0) {
        printf("Error opening file %s\n", group_filename);
        exit(-1);
    }
    if((passwd_file = open(passwd_filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR |
                          S_IRGRP | S_IROTH)) < 0) {
        printf("Error opening file %s\n", passwd_filename);
        exit(-1);
    }

    /* write query at the top of the file */
    query_length = strlen(query);
    write(trace_file, &query_length, sizeof(size_t));
    write(trace_file, query, query_length*sizeof(char));

    write(trace_file, &num_rows, sizeof(unsigned long long));

    if (use_dependencies) {
        count_dep = count_dependencies_jobs(dep_filename);
        load_dependency(dep_filename, count_dep);
    }

    npreset_jobs=0;
    while ((row = mysql_fetch_row(result_job))) {

        /*for( i = 0; i < num_fields; i++) {
            printf("%s ", row[i] ? row[i] : "NULL");
        }
        printf("\n");*/

        job_trace.preset = 0;
        sprintf(job_trace.account, "%s", row[0]);
        job_trace.exit_code = atoi(row[1]);
        sprintf(job_trace.job_name, "%s", row[2]);
        job_trace.id_job = atoi(row[3]);
        sprintf(job_trace.qos_name, "%s", row[4]);
        job_trace.id_user = atoi(row[5]);
        job_trace.id_group = atoi(row[6]);
        if (row[7] != NULL) {
            sprintf(job_trace.resv_name, "%s", row[7]);
        } else {
            job_trace.resv_name[0] = '\0';;
        }
        sprintf(job_trace.nodelist, "%s", row[8]);
        job_trace.nodes_alloc = atoi(row[9]);
        sprintf(job_trace.partition, "%s", row[10]);
        job_trace.state = atoi(row[11]);
        job_trace.timelimit = atoi(row[12]);
        job_trace.time_submit = strtol(row[13], NULL, 0);
        job_trace.time_eligible = strtol(row[14], NULL, 0);
        job_trace.time_start = strtol(row[15], NULL, 0);
        job_trace.time_end = strtol(row[16], NULL, 0);
        if (job_trace.time_submit < time_start) {
            job_trace.time_submit = time_start;
            if (!nopreset && job_trace.time_start < time_start) {
                //preset jobs are jobs with a submit time and start time before the trace start time
                job_trace.preset = 1;
                npreset_jobs++;
            }
        }
        if (job_trace.time_eligible < time_start) {
            job_trace.time_eligible = time_start;
        }
        //runaway job
        if (job_trace.time_start == 0) {
            job_trace.time_start = job_trace.time_end;
        }
        //runaway job 2
        if (job_trace.time_eligible > job_trace.time_start) {
            job_trace.time_eligible = job_trace.time_start;
        }
        if (job_trace.time_start < time_start) {
            job_trace.time_start = time_start;
        }
        job_trace.time_suspended = strtol(row[17], NULL, 0);
        strcpy(job_trace.gres_alloc, reconstruct_constraint(row[18]));
        job_trace.priority = atoi(row[19]);

        //printf("[%d] submit=%ld start=%ld end=%ld\n", job_trace.id_job,  job_trace.time_submit, job_trace.time_start, job_trace.time_end);
        if (use_dependencies) {
            lookfor(job_trace.id_job, num_rows, job_trace.dependencies, &(job_trace.switches));
        } else {
            job_trace.dependencies[0] = '\0';
            job_trace.switches = 0;
        }

        written = write(trace_file, &job_trace, sizeof(job_trace_t));
        if(written != sizeof(job_trace_t)) {
            printf("Error writing to file: %d of %ld\n", written, sizeof(job_trace_t));
            exit(-1);
        }

        sprintf(group_item, "%s:%d\n", job_trace.account, job_trace.id_group);
        written = write(group_file, &group_item, strlen(group_item));
        if(written != strlen(group_item)) {
            printf("Error writing to file: %d of %ld\n", written, strlen(group_item));
            exit(-1);
        }

        sprintf(user_item, "%s:%d:%d\n", row[20], job_trace.id_user, job_trace.id_group);
        written = write(passwd_file, &user_item, strlen(user_item));
        if(written != strlen(user_item)) {
            printf("Error writing to file: %d of %ld\n", written, strlen(user_item));
            exit(-1);
        }
    }

    printf("\nSuccessfully written file %s : Total number of jobs = %llu, preset jobs = %llu\n", filename, num_rows, npreset_jobs);
    mysql_free_result(result_job);

    // process reservation data
    if (do_resv) {
        memset(query,'\0',1024);
        sprintf(query, "SELECT r.id_resv, r.time_start, r.time_end, r.nodelist, r.resv_name, GROUP_CONCAT(DISTINCT a.acct), r.flags, r.tres "
                "FROM %s AS r INNER JOIN %s AS a ON FIND_IN_SET(a.id_assoc,r.assoclist) "
                "WHERE r.time_start < %lu AND r.time_end > %lu  AND r.time_end - r.time_start < 15770000 "
                "GROUP BY r.time_start, r.id_resv ORDER BY r.time_start", resv_table, assoc_table, time_end, time_start);

        printf("\nQuery --> %s\n\n", query);

        if (mysql_query(conn, query)) {
            finish_with_error(conn);
        }

        result_resv = mysql_store_result(conn);
        if (result_resv == NULL) {
            finish_with_error(conn);
        }

        num_rows = mysql_num_rows(result_resv);
        write(trace_file, &num_rows, sizeof(unsigned long long));

        while ((row = mysql_fetch_row(result_resv))) {

            resv_trace.id_resv = atoi(row[0]);
            resv_trace.time_start = strtol(row[1], NULL, 0);
            if (resv_trace.time_start < time_start) {
                resv_trace.time_start = time_start;
                resv_trace.preset = 1;
            }
            resv_trace.time_end = strtol(row[2], NULL, 0);
            //if (resv_trace.time_end > time_end) {
            //    resv_trace.time_end = time_end;
            //}
            sprintf(resv_trace.nodelist, "%s", row[3]);
            sprintf(resv_trace.resv_name, "%s", row[4]);
            sprintf(resv_trace.accts, "%s", row[5]);
            resv_trace.flags = atoi(row[6]);
            sprintf(resv_trace.tres, "%s", row[7]);

            written = write(trace_file, &resv_trace, sizeof(resv_trace_t));
            if(written != sizeof(resv_trace_t)) {
                printf("Error writing to file: %d of %ld\n", written, sizeof(resv_trace_t));
                exit(-1);
            }
        }
        printf("\nSuccessfully written file %s : Total number of reservations = %llu\n", filename, num_rows);
        mysql_free_result(result_resv);
    }

    // process event data
    if (do_event) {
        memset(query,'\0',1024);
        snprintf(query, 1024*sizeof(char), "SELECT e.time_start, e.time_end, e.node_name, e.reason, e.state "
                 "FROM %s AS e "
                 "WHERE e.time_start < %lu AND e.time_end > %lu AND e.node_name <> '' "
                 "ORDER BY e.node_name, e.time_end", event_table, time_end, time_start);

        printf("\nQuery --> %s\n\n", query);
        if (mysql_query(conn, query)) {
            finish_with_error(conn);
        }

        result_node = mysql_store_result(conn);
        if (result_node == NULL) {
            finish_with_error(conn);
        }

        num_rows = mysql_num_rows(result_node);
        cur_offset = lseek(trace_file, 0, SEEK_CUR);
        w_num_rows = k = 0;
        write(trace_file, &num_rows, sizeof(unsigned long long));

        memset(&fix_node_trace, 0, sizeof(node_trace_t));
        while ((row = mysql_fetch_row(result_resv))) {
            //printf("read %s %s %s\n",row[0], row[1], row[2]);
            k++;
            if (fix_node_trace.time_start == 0) {
                fix_node_trace.time_start = strtol(row[0], NULL, 0);
                fix_node_trace.time_end = strtol(row[1], NULL, 0);
                sprintf(fix_node_trace.node_name, "%s", row[2]);
                sprintf(fix_node_trace.reason, "%s", row[3]);
                fix_node_trace.state = atoi(row[4]);
            } else {
                if (fix_node_trace.time_end == strtol(row[0],NULL,0) &&
                    strcmp(fix_node_trace.node_name, row[2]) == 0 &&
                    strcmp(fix_node_trace.reason, row[3]) == 0) {
                    fix_node_trace.time_end = strtol(row[1], NULL, 0);
                    if (atoi(row[4]) > fix_node_trace.state) {
                        fix_node_trace.state = atoi(row[4]);
                    }
                    if (k == num_rows) {
                        // special case for last one
                        if (fix_node_trace.time_start < time_start) {
                            fix_node_trace.time_start = time_start;
                            fix_node_trace.preset = 1;
                        }
                        //if (fix_node_trace.time_end > time_end) {
                        //    fix_node_trace.time_end = time_end;
                        //}
                        written = write(trace_file, &fix_node_trace, sizeof(node_trace_t));
                        if(written != sizeof(node_trace_t)) {
                            printf("Error writing to file: %d of %ld\n", written, sizeof(node_trace_t));
                            exit(-1);
                        }
                        //printf("write %d %d %s\n",fix_node_trace.time_start, fix_node_trace.time_end, fix_node_trace.node_name);
                        w_num_rows++;
                    }
                } else {
                    // event to write
                    if (fix_node_trace.time_start < time_start) {
                        fix_node_trace.time_start = time_start;
                        fix_node_trace.preset = 1;
                    }
                    //if (fix_node_trace.time_end > time_end) {
                    //    fix_node_trace.time_end = time_end;
                    //}
                    written = write(trace_file, &fix_node_trace, sizeof(node_trace_t));
                    if(written != sizeof(node_trace_t)) {
                        printf("Error writing to file: %d of %ld\n", written, sizeof(node_trace_t));
                        exit(-1);
                    }
                    //printf("write %d %d %s\n",fix_node_trace.time_start, fix_node_trace.time_end, fix_node_trace.node_name);
                    w_num_rows++;
                    fix_node_trace.time_start = strtol(row[0], NULL, 0);
                    fix_node_trace.time_end = strtol(row[1], NULL, 0);
                    sprintf(fix_node_trace.node_name, "%s", row[2]);
                    sprintf(fix_node_trace.reason, "%s", row[3]);
                    fix_node_trace.state = atoi(row[4]);
                    if (k == num_rows) {
                        // special case for last one
                        if (fix_node_trace.time_start < time_start) {
                            fix_node_trace.time_start = time_start;
                            fix_node_trace.preset = 1;
                        }
                        //if (fix_node_trace.time_end > time_end) {
                        //    fix_node_trace.time_end = time_end;
                        //}
                        written = write(trace_file, &fix_node_trace, sizeof(node_trace_t));
                        if(written != sizeof(node_trace_t)) {
                            printf("Error writing to file: %d of %ld\n", written, sizeof(node_trace_t));
                            exit(-1);
                        }
                        //printf("write %d %d %s\n",fix_node_trace.time_start, fix_node_trace.time_end, fix_node_trace.node_name);
                        w_num_rows++;
                    }
                }
            }
        }

        // update number of node events
        lseek(trace_file, cur_offset, SEEK_SET);
        write(trace_file, &w_num_rows, sizeof(unsigned long long));

        printf("\nSuccessfully written file %s : Total number of events = %llu\n", filename, num_rows);
        mysql_free_result(result_node);
    }

    if (use_dependencies) {
        free(depend);
    }

    mysql_close(conn);
    exit(0);
}
