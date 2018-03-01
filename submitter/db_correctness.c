#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#define __USE_XOPEN
#include <time.h>

#include "trace.h"

char *host = NULL;
char *dbname = NULL;
char *job_table = NULL;
char *step_table = NULL;
char *user = NULL;
char *password;

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
    -d, --dbname     db_name         Name of the database\n\
    -h, --host       db_hostname     Name of machine hosting MySQL DB\n\
    -u, --user       dbuser          Name of user with which to establish a\n\
                                     connection to the DB\n\
    -p, --password   password        Password to connect to the db\n\
    -t, --job_table  db_job_table    Name of the MySQL table to query to obtain job information\n\
    -s, --step_table db_step_table   Name of the MySQL table to query to obtain step infomartion\n\
    -?, --help                       This help message\n\n\
");
}

static void
get_args(int argc, char** argv)
{
    static struct option long_options[] = {
        {"host", required_argument, 0, 'h'},
        {"dbname", required_argument, 0, 'd'},
        {"help", no_argument,   0, '?'},
        {"password", required_argument,   0, 'p'},
        {"job_table", required_argument, 0, 't'},
        {"step_table", required_argument, 0, 's'},
        {"user", required_argument, 0, 'u'},
        {0, 0, 0, 0}
    };
    int opt_char, option_index;

    while(1) {
        if ((opt_char = getopt_long(argc, argv, "d:h:?s:t:u:p:", long_options, &option_index)) == -1 )
            break;
        switch  (opt_char) {
        case ('p'):
            password = strdup(optarg);
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
            step_table = optarg;
            break;
        case ('t'):
            job_table = optarg;
            break;
        case ('u'):
            user = optarg;
            break;
        };
    }
}

int main(int argc, char **argv)
{
    char query[1024];
    unsigned int num_rows = 0;

    MYSQL *conn;
    MYSQL_RES *result_upd = NULL;

    get_args(argc, argv);

    if ((user == NULL) || (host == NULL) || (dbname == NULL) || (job_table == NULL) || (step_table == NULL)) {
        printf("user, host, dbname and tables cannot be NULL!\n");
        print_usage();
        exit(-1);
    }

    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, host, user, password, dbname, 0, NULL, 0)) {
        finish_with_error(conn);
    }

    memset(query,'\0',1024);
    sprintf(query,"UPDATE %s AS j JOIN %s AS s "
            "ON s.job_db_inx=j.job_db_inx "
            "SET j.time_start = s.time_start "
            "WHERE j.time_start=0",
            job_table, step_table);
    printf("\nQuery --> %s\n\n", query);

    if (mysql_query(conn, query)) {
        finish_with_error(conn);
    }
    num_rows = mysql_affected_rows(conn);
    printf("\nCorrecting db, number of updated fields: %llu\n", num_rows);
    mysql_free_result(result_upd);

    mysql_close(conn);
    exit(0);
}
