#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <fcntl.h>
#include <string.h>
#include "trace.h"
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>

#define DEFAULT_START_TIMESTAMP 1420066800
static const char DEFAULT_START[] = "2015-01-01 00:00:00";


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
    -s, --starttime time            Start selecting jobs from this time\n\
                                    format: \"yyyy-MM-DD hh:mm:ss\"\n\
    -e, --endtime   time            Stop selecting jobs at this time\n\
                                    format: \"yyyy-MM-DD hh:mm:ss\"\n\
    -d, --dbname    db_name         Name of the database\n\
    -h, --host      db_hostname     Name of machine hosting MySQL DB\n\
    -u, --user      dbuser          Name of user with which to establish a\n\
                                    connection to the DB\n\
    -p, --password  password        Password to connect to the db\n\
    -t, --table     db_table        Name of the MySQL table to query\n\
    -v, --verbose                   Increase verbosity of the messages\n\
    -f, --file      filename        Name of the output trace file being created\n\
    -q, --query     query           Use a SQL query to retrieve the data\n\
    -?, --help                      This help message\n\n\
");
}

int
main(int argc, char **argv)
{

    int i,c,written;
    unsigned long njobs = 0;
    int trace_file;
    int use_query = 0;
    char year[4], month[2], day[2], hours[2], minutes[2], seconds[2];

    char *endtime = NULL;
    char *filename = NULL;
    char *host = NULL;
    char *dbname = NULL;
    char *starttime = NULL;
    char *table = NULL;
    char *user = NULL;
    char *query = NULL;
    char *password;

    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    size_t query_length;
    job_trace_t new_trace;


    while(1) { 
        static struct option long_options[] = {
            {"endtime", required_argument, 0, 'e'},
            {"file", required_argument, 0, 'f'},
            {"host", required_argument, 0, 'h'},
            {"dbname", required_argument, 0, 'd'},
            {"help", no_argument,   0, '?'},
            {"password", required_argument,   0, 'p'},
            {"query", required_argument,   0, 'q'},
            {"starttime", required_argument, 0, 's'},
            {"table", required_argument, 0, 't'},
            {"user", required_argument, 0, 'u'},
            {0, 0, 0, 0}
        }; 

        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long (argc, argv, "d:e:f:h:?s:t:u:pq:",long_options,
                         &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;

        switch (c) {
        case 'p':
            password = optarg;
            break;
        case 'q':
            use_query = 1;
            query = optarg;
            break;
        case 'e':
            endtime = optarg;
            break;
        case 'f':
            filename = optarg;
            break;
        case 'h':
            host = optarg;
            break;
        case 'd':
            dbname = optarg;
            break;
        case '?':
            print_usage();
            exit(0);
        case 's':
            starttime = optarg;
            break;
        case 't':
            table = optarg;
            break;
        case 'u':
            user = optarg;
            break;
        default:
            print_usage();
            abort ();
        }
    } /* while */

    if ((user == NULL) || (host == NULL) || (dbname == NULL) || (filename == NULL)) {
        printf("user, host, dbname and trace file name cannot be NULL!\n");
        print_usage();
        exit(-1);
    }

    if (!use_query) {
        query = malloc(1024*sizeof(char));

        /*validate input parameter to build the query*/
        if ((endtime == NULL) || (starttime == NULL) || (table == NULL)) {
            printf("endtime, starttime and table cannot be NULL!\n");
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
                 "t.id_job, t.id_user, t.id_group, t.mem_req, t.nodelist, t.nodes_alloc, t.partition, t.priority, t.state, "
                 "t.timelimit, t.time_submit, t.time_eligible, t.time_start, t.time_end, t.time_suspended, t.gres_req, t.gres_alloc, t.tres_req "
                 "FROM %s as t "
                 "WHERE FROM_UNIXTIME(t.time_submit) BETWEEN '%s' AND '%s' AND "
                 "t.time_end > 0 AND t.nodes_alloc > 0 AND t.partition='normal'", table, starttime, endtime);
    }
    printf("\nQuery --> %s\n\n", query);

    conn = mysql_init(NULL);

    if (!mysql_real_connect(conn, host, user, password, dbname, 0, NULL, 0)) {
        finish_with_error(conn);
    }

    if (mysql_query(conn, query)) {
        finish_with_error(conn);
    }
    res = mysql_store_result(conn);

    if (res == NULL) {
        finish_with_error(conn);
    }

    int num_fields = mysql_num_fields(res);

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

    while ((row = mysql_fetch_row(res))) {

        /*for( i = 0; i < num_fields; i++) {
            printf("%s ", row[i] ? row[i] : "NULL");
        }
        printf("\n");*/
        njobs++;

        sprintf(new_trace.account, "%s", row[0]);
        new_trace.cpus_req = atoi(row[1]);
        new_trace.exit_code = atoi(row[2]);
        sprintf(new_trace.job_name, "%s", row[3]);
        new_trace.id_job = atoi(row[4]);
        new_trace.id_user = atoi(row[5]);
        new_trace.id_group = atoi(row[6]);
        new_trace.mem_req = strtoul(row[7], NULL, 0);
        sprintf(new_trace.nodelist, "%s", row[8]);
        new_trace.nodes_alloc = atoi(row[9]);
        sprintf(new_trace.partition, "%s", row[10]);
        new_trace.priority = atoi(row[11]);
        new_trace.state = atoi(row[12]);
        new_trace.timelimit = atoi(row[13]);
        new_trace.time_submit = strtoul(row[14], NULL, 0);
        new_trace.time_eligible = strtoul(row[15], NULL, 0);
        new_trace.time_start = strtoul(row[16], NULL, 0);
        new_trace.time_end = strtoul(row[17], NULL, 0);
        new_trace.time_suspended = strtoul(row[18], NULL, 0);
        sprintf(new_trace.gres_req, "%s", row[19]);
        sprintf(new_trace.gres_alloc, "%s", row[20]);
        sprintf(new_trace.tres_req, "%s", row[21]);

        written = write(trace_file, &new_trace, sizeof(new_trace));
        if(written != sizeof(new_trace)) {
            printf("Error writing to file: %d of %ld\n", written,
                   sizeof(new_trace));
            exit(-1);
        }

    }

    printf("\nSuccessfully written file %s : Total number of jobs = %ld\n", filename, njobs);

    /* close connection */
    mysql_free_result(res);
    mysql_close(conn);
    if (!use_query) {
        free(query);
    }

    exit(0);
}
