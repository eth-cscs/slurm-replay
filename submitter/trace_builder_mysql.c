#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "trace.h"

char *endtime = NULL;
char *filename = NULL;
char *host = NULL;
char *dbname = NULL;
char *starttime = NULL;
char *job_table = NULL;
char *resv_table = NULL;
char *assoc_table = NULL;
char *event_table = NULL;
char *user = NULL;
char *password;
char *query = NULL;
static int use_query = 0;


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
    -u, --user       dbuser          Name of user with which to establish a\n\
                                     connection to the DB\n\
    -p, --password   password        Password to connect to the db\n\
    -t, --job_table  db_job_table    Name of the MySQL table to query to obtain job information\n\
    -r, --resv_table db_resv_table   Name of the MySQL table to query to obtain reservation infomartion\n\
    -a, --assoc_table db_assoc_table   Name of the MySQL table to query to obtain associationss infomartion\n\
    -v, --event_table db_event_table   Name of the MySQL table to query to obtain event information about node availability\n\
    -f, --file       filename        Name of the output trace file being created\n\
    -q, --query      query           Use a SQL query to retrieve the data\n\
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
        {"query", required_argument,   0, 'q'},
        {"starttime", required_argument, 0, 's'},
        {"job_table", required_argument, 0, 't'},
        {"resv_table", required_argument, 0, 'r'},
        {"assoc_table", required_argument, 0, 'a'},
        {"event_table", required_argument, 0, 'v'},
        {"user", required_argument, 0, 'u'},
        {0, 0, 0, 0}
    };
    int opt_char, option_index;

    while(1) {
        if ((opt_char = getopt_long(argc, argv, "d:e:f:h:?s:t:r:u:p:q:a:v:", long_options, &option_index)) == -1 )
            break;
        switch  (opt_char) {
        case ('p'):
            password = strdup(optarg);
            break;
        case ('q'):
            use_query = 1;
            query = optarg;
            break;
        case ('e'):
            endtime = optarg;
            break;
        case ('f'):
            filename = optarg;
            break;
        case ('h'):
            host = optarg;
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
        case ('t'):
            job_table = optarg;
            break;
        case ('r'):
            resv_table = optarg;
            break;
        case ('a'):
            assoc_table = optarg;
            break;
        case ('v'):
            event_table = optarg;
            break;
        case ('u'):
            user = optarg;
            break;
        };
    }
}

int main(int argc, char **argv)
{

    int c,written;
    int trace_file;
    char year[4], month[2], day[2], hours[2], minutes[2], seconds[2];

    size_t query_length;
    //unsigned int num_fields;
    unsigned long long num_rows;
    unsigned long long w_num_rows, k;

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

    if ((user == NULL) || (host == NULL) || (dbname == NULL) || (filename == NULL) || (job_table == NULL) || (resv_table == NULL)|| (event_table == NULL)) {
        printf("user, host, dbname and trace file name cannot be NULL!\n");
        print_usage();
        exit(-1);
    }

    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, host, user, password, dbname, 0, NULL, 0)) {
        finish_with_error(conn);
    }

    if (!use_query) {
        query = malloc(1024*sizeof(char));
        memset(query,'\0',1024);

        /*validate input parameter to build the query*/
        if ((endtime == NULL) || (starttime == NULL) || (job_table == NULL)) {
            printf("endtime, starttime and job table cannot be NULL!\n");
            print_usage();
            exit(-1);
        }

        if ( sscanf(endtime, "%4s-%2s-%2s %2s:%2s:%2s", year, month, day, hours,
                    minutes, seconds) != 6 ) {
            printf("ERROR validate endtime\n");
            print_usage();
            exit(-1);
        }


        if ( sscanf(starttime, "%4s-%2s-%2s %2s:%2s:%2s", year, month, day,
                    hours, minutes, seconds) != 6 ) {
            printf("ERROR validate starttime\n");
            print_usage();
            exit(-1);
        }
        snprintf(query, 1024*sizeof(char), "SELECT t.account, t.cpus_req, t.exit_code, t.job_name, "
                 "t.id_job, t.id_user, t.id_group, r.resv_name, t.mem_req, t.nodelist, t.nodes_alloc, t.partition, t.priority, t.state, "
                 "t.timelimit, t.time_submit, t.time_eligible, t.time_start, t.time_end, t.time_suspended, "
                 "t.gres_req, t.gres_alloc, t.tres_req "
                 "FROM %s as t LEFT JOIN %s as r ON t.id_resv = r.id_resv AND t.time_start >= r.time_start and t.time_end <= r.time_end "
                 "WHERE FROM_UNIXTIME(t.time_submit) BETWEEN '%s' AND '%s' AND "
                 "t.time_end > 0 AND t.nodes_alloc > 0 AND t.partition='normal'", job_table, resv_table, starttime, endtime);
    }
    printf("\nQuery --> %s\n\n", query);

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

    /* write query at the top of the file */
    query_length = strlen(query);
    write(trace_file, &query_length, sizeof(size_t));
    write(trace_file, query, query_length*sizeof(char));

    write(trace_file, &num_rows, sizeof(unsigned long long));

    while ((row = mysql_fetch_row(result_job))) {

        /*for( i = 0; i < num_fields; i++) {
            printf("%s ", row[i] ? row[i] : "NULL");
        }
        printf("\n");*/

        sprintf(job_trace.account, "%s", row[0]);
        job_trace.cpus_req = atoi(row[1]);
        job_trace.exit_code = atoi(row[2]);
        sprintf(job_trace.job_name, "%s", row[3]);
        job_trace.id_job = atoi(row[4]);
        job_trace.id_user = atoi(row[5]);
        job_trace.id_group = atoi(row[6]);
        if (row[7] != NULL) {
            sprintf(job_trace.resv_name, "%s", row[7]);
        } else {
            job_trace.resv_name[0] = '\0';;
        }
        job_trace.mem_req = strtoul(row[8], NULL, 0);
        sprintf(job_trace.nodelist, "%s", row[9]);
        job_trace.nodes_alloc = atoi(row[10]);
        sprintf(job_trace.partition, "%s", row[11]);
        job_trace.priority = atoi(row[12]);
        job_trace.state = atoi(row[13]);
        job_trace.timelimit = atoi(row[14]);
        job_trace.time_submit = strtoul(row[15], NULL, 0);
        job_trace.time_eligible = strtoul(row[16], NULL, 0);
        job_trace.time_start = strtoul(row[17], NULL, 0);
        job_trace.time_end = strtoul(row[18], NULL, 0);
        job_trace.time_suspended = strtoul(row[19], NULL, 0);
        sprintf(job_trace.gres_req, "%s", row[20]);
        sprintf(job_trace.gres_alloc, "%s", row[21]);
        sprintf(job_trace.tres_req, "%s", row[22]);

        written = write(trace_file, &job_trace, sizeof(job_trace_t));
        if(written != sizeof(job_trace_t)) {
            printf("Error writing to file: %d of %ld\n", written, sizeof(job_trace_t));
            exit(-1);
        }

    }

    printf("\nSuccessfully written file %s : Total number of jobs = %ld\n", filename, num_rows);
    mysql_free_result(result_job);
    if (!use_query) {
        free(query);
    }

    // process reservation data
    if (!use_query && resv_table != NULL && assoc_table != NULL) {
        memset(query,'\0',1024);
        snprintf(query, 1024*sizeof(char), "SELECT r.id_resv, r.time_start, r.time_end, r.nodelist, r.resv_name, GROUP_CONCAT(DISTINCT a.acct), r.flags, r.tres "
                 "FROM %s AS r INNER JOIN %s AS a ON FIND_IN_SET(a.id_assoc,r.assoclist) "
                 "WHERE FROM_UNIXTIME(r.time_start) BETWEEN '%s' AND '%s' GROUP BY r.time_start, r.id_resv ORDER BY r.time_start", resv_table, assoc_table, starttime, endtime);

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
            resv_trace.time_start = strtoul(row[1], NULL, 0);
            resv_trace.time_end = strtoul(row[2], NULL, 0);
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
        mysql_free_result(result_resv);
        printf("\nSuccessfully written file %s : Total number of reservations = %ld\n", filename, num_rows);
    }

    // process event data
    if (!use_query && event_table != NULL) {
        memset(query,'\0',1024);
        snprintf(query, 1024*sizeof(char), "SELECT e.time_start, e.time_end, e.node_name, e.reason, e.state "
                 "FROM %s AS e "
                 "WHERE FROM_UNIXTIME(e.time_start) BETWEEN '%s' AND '%s' ORDER BY e.node_name, e.time_end", event_table, starttime, endtime);

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
                fix_node_trace.time_start = strtoul(row[0], NULL, 0);
                fix_node_trace.time_end = strtoul(row[1], NULL, 0);
                sprintf(fix_node_trace.node_name, "%s", row[2]);
                sprintf(fix_node_trace.reason, "%s", row[3]);
                fix_node_trace.state = atoi(row[4]);
            } else {
                if (fix_node_trace.time_end == strtoul(row[0],NULL,0) &&
                    strcmp(fix_node_trace.node_name, row[2]) == 0 &&
                    strcmp(fix_node_trace.reason, row[3]) == 0) {
                    fix_node_trace.time_end = strtoul(row[1], NULL, 0);
                    if (atoi(row[4]) > fix_node_trace.state) {
                        fix_node_trace.state = atoi(row[4]);
                    }
                    if (k == num_rows) {
                        // special case for last one
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
                    written = write(trace_file, &fix_node_trace, sizeof(node_trace_t));
                    if(written != sizeof(node_trace_t)) {
                        printf("Error writing to file: %d of %ld\n", written, sizeof(node_trace_t));
                        exit(-1);
                    }
                    //printf("write %d %d %s\n",fix_node_trace.time_start, fix_node_trace.time_end, fix_node_trace.node_name);
                    w_num_rows++;
                    fix_node_trace.time_start = strtoul(row[0], NULL, 0);
                    fix_node_trace.time_end = strtoul(row[1], NULL, 0);
                    sprintf(fix_node_trace.node_name, "%s", row[2]);
                    sprintf(fix_node_trace.reason, "%s", row[3]);
                    fix_node_trace.state = atoi(row[4]);
                    if (k == num_rows) {
                        // special case for last one
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

        mysql_free_result(result_node);
        printf("\nSuccessfully written file %s : Total number of events = %ld\n", filename, num_rows);
    }

    mysql_close(conn);
    exit(0);
}
