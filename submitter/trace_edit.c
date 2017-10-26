#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "trace.h"

/* This command modifies a job trace file */

static struct option long_options[] = {
    {"job_id",             1, 0, 'j'},
    {"username",           1, 0, 'u'},
    {"submit",             1, 0, 's'},
    {"duration",           1, 0, 'd'},
    {"wclimit",            1, 0, 'w'},
    {"tasks",              1, 0, 't'},
    {"qosname",            1, 0, 'q'},
    {"partition",          1, 0, 'p'},
    {"account",            1, 0, 'a'},
    {"cpus_per_task",      1, 0, 'c'},
    {"tasks_per_node",     1, 0, 'n'},
    {"reservation",        1, 0, 'r'},
    {"dependency",         1, 0, 'e'},
    {"index",              1, 0, 'x'},
    {"new_job_id",         1, 0, 'J'},
    {"new_username",       1, 0, 'U'},
    {"new_submit",         1, 0, 'S'},
    {"new_duration",       1, 0, 'D'},
    {"new_wclimit",        1, 0, 'W'},
    {"new_tasks",          1, 0, 'T'},
    {"new_qosname",        1, 0, 'Q'},
    {"new_partition",      1, 0, 'P'},
    {"new_account",        1, 0, 'A'},
    {"new_cpus_per_task",  1, 0, 'C'},
    {"new_tasks_per_node", 1, 0, 'N'},
    {"new_reservation",    1, 0, 'R'},
    {"new_dependency",     1, 0, 'E'},
    {"remove_jobs",        0, 0, 'X'},
    {"help",               0, 0, 'h'},
    {"wrkldfile",          1, 0, 'i'},
    {"insert",             1, 0, 'I'},
    {"sort",               0, 0, 'O'},
    {NULL,                 1, 0, 0  }
};

void fill_in_new_val();
void init_record(job_trace_t*  jrec);
void copy_to_new_val();
void print_record(char* label, job_trace_t* jrec);
int  sort_file(int trace_file, int new_file);
int  process_trace_file(int trace_file, int new_file);

job_trace_t rec_pat, new_val, job_trace, upd_val;

/*
 * Types of value that any new submit time provided could be.
 * ABSOLUTE: The value provided is itself the value to use.
 * RELATIVE: The job's new submit time should be the old value
 *           plus the one provided. (Could be negative).
 */
enum {
    ABSOLUTE = 1,
    RELATIVE = 2
};

int delete_job = 0;
int insert_job = 0;
int sortit     = 0;
int select_idx = -1;  /* Default--not specified */
int submit_time_type = ABSOLUTE;

/* Name of the file containing the workload to simulate */
char*  trace_file_name      = NULL;
char   default_trace_file[] = "test.trace";


char help_msg[] = "\
edit_trace [OPTIONS]\n\
  -j, --job_id             jobid Select records with job_id=joid\n\
  -u, --username           name  Select records with username=name\n\
  -s, --submit             time  Select records with submit=time\n\
  -d, --duration           secs  Select records with duration=secs\n\
  -w, --wclimit            timeh Select records with wclimit=timeh\n\
  -t, --tasks              num   Select records with tasks=num\n\
  -q, --qosname            qos   Select records with qosname=qos\n\
  -p, --partition          par   Select records with partition=par\n\
  -a, --account            acc   Select records with account=acc\n\
  -c, --cpus_per_task      cpt   Select records with cpus_per_task=cpt\n\
  -n, --tasks_per_node     tpn   Select records with tasks_per_node=tpn\n\
  -r, --reservation        res   Select records with reservation=res\n\
  -e, --dependency         dep   Select records with dependency=dep\n\
  -x, --index              idx   Select record number idx\n\
  -J, --new_job_id         jobid Set job_id to jobid in all matched records\n\
  -U, --new_username       name  Set username to name in all matched records\n\
  -S, --new_submit         time  Set submit to time in all matched records\n\
  -D, --new_duration       secs  Set duration to secs in all matched records\n\
  -W, --new_wclimit        timeh Set wclimit to timeh in all matched records\n\
  -T, --new_tasks          num   Set tasks to num in all matched records\n\
  -Q, --new_qosname        qos   Set qosname to qos in all matched records\n\
  -P, --new_partition      par   Set partition to par in all matched records\n\
  -A, --new_account        acc   Set account to acc in all matched records\n\
  -C, --new_cpus_per_task  cpt   Set cpus_per_task to cpt in all matched\n\
                                 records\n\
  -N, --new_tasks_per_node tpn   Set tasks_per_node to tpn in all matched\n\
                                 records\n\
  -R, --new_reservation    res   Set reservation to res in all matched records\n\
  -E, --new_dependency     dep   Set dependency to dep in all matched records\n\
  -X, --remove_jobs              Delete all matched records\n\
  -h, --help                     This help message\n\
  -i, --wrkldfile          name  Name of the trace file to edit\n\
  -I, --insert                   Insert a record after each matched record\n\
  -O, --sort                     Sort all records in ascending\n\
                                 chronological order\n\
\n\nNotes:  The edit_trace utility consists of two general sets of options.\n\
        The first is the set of all options used to specify which existing\n\
        records to select.  These have lower-case short options.  The second\n\
        is the set of all options used to specify what the new values should\n\
        be.  These have capital-case short options and their long forms are\n\
        prefixed with 'new_'.\n\
        If sorting, all other edits are performed first and then the result\n\
        is sorted.\n\
        Job id's will be out-of-order after chronological sort.";

char optstr[] = "j:u:s:d:w:t:q:p:a:c:n:r:e:x:J:U:S:D:W:T:Q:P:A:C:N:R:E:Xhi:IO";

void
process_args(int argc, char* argv[])
{
    int opt_char, option_index;
    while((opt_char = getopt_long(argc, argv, optstr,
                                  long_options, &option_index)) != -1) {
        switch (opt_char) {
        case (int)'j':
            rec_pat.job_id = atoi(optarg);
            break;
        case (int)'u':
            strcpy(rec_pat.username,optarg);
            break;
        case (int)'s':
            rec_pat.submit = atol(optarg);
            break;
        case (int)'d':
            rec_pat.duration = atoi(optarg);
            break;
        case (int)'w':
            rec_pat.wclimit = atoi(optarg);
            break;
        case (int)'t':
            rec_pat.tasks = atoi(optarg);
            break;
        case (int)'q':
            strcpy(rec_pat.qosname,optarg);
            break;
        case (int)'p':
            strcpy(rec_pat.partition,optarg);
            break;
        case (int)'a':
            strcpy(rec_pat.account,optarg);
            break;
        case (int)'c':
            rec_pat.cpus_per_task = atoi(optarg);
            break;
        case (int)'n':
            rec_pat.tasks_per_node = atoi(optarg);
            break;
        case (int)'r':
            strcpy(rec_pat.reservation,optarg);
            break;
        case (int)'e':
            strcpy(rec_pat.dependency,optarg);
            break;
        case (int)'x':
            select_idx = atoi(optarg);
            break;
        case (int)'J':
            upd_val.job_id = atoi(optarg);
            break;
        case (int)'U':
            strcpy(upd_val.username,optarg);
            break;
        case (int)'S':
            if (optarg[0] == '+' || optarg[0] == '-') {
                printf("New time relative.\n");
                submit_time_type = RELATIVE;
            } else {
                printf("New time appears to be absolute.\n");
            }
            upd_val.submit = atol(optarg);
            break;
        case (int)'D':
            upd_val.duration = atoi(optarg);
            break;
        case (int)'W':
            upd_val.wclimit = atoi(optarg);
            break;
        case (int)'T':
            upd_val.tasks = atoi(optarg);
            break;
        case (int)'Q':
            strcpy(upd_val.qosname,optarg);
            break;
        case (int)'P':
            strcpy(upd_val.partition,optarg);
            break;
        case (int)'A':
            strcpy(upd_val.account,optarg);
            break;
        case (int)'C':
            upd_val.cpus_per_task = atoi(optarg);
            break;
        case (int)'N':
            upd_val.tasks_per_node = atoi(optarg);
            break;
        case (int)'R':
            strcpy(upd_val.reservation,optarg);
            break;
        case (int)'E':
            strcpy(upd_val.dependency,optarg);
            break;
        case (int)'X':
            delete_job = 1;
            break;
        case (int)'i':
            trace_file_name = strdup(optarg);
            break;
        case (int)'h':
            printf("%s\n", help_msg);
            exit(0);
        case (int)'I':
            insert_job = 1;
            /* In this case, we simply write out the
             * originaly record followed by the possibly updated
             * record.  Thus, if the user doesn't specify any
             * modification options, it will be a duplicate.
             * Each record match has a new record inserted after it.
             */
            break;
        case (int)'O':
            sortit = 1;
            break;
        };
    }
}

int
main(int argc, char *argv[])
{
    int trace_file, new_file;
    int errs = 0;
    char new_file_name[64] = { ".test.trace.new" };

    /* Initialize the pattern-matching record to indicate all fields unused. */
    init_record(&rec_pat);
    init_record(&upd_val);

    process_args(argc, argv);

    if (!trace_file_name) trace_file_name = default_trace_file;

    trace_file = open(trace_file_name, O_RDONLY);
    if(trace_file < 0) {
        printf("Error opening %s!\n", trace_file_name);
        return -1;
    }

    new_file = open(new_file_name, O_CREAT | O_RDWR, S_IRWXU);
    if(new_file < 0) {
        printf("Error creating temporary file!\n");
        return -1;
    }

    errs = process_trace_file(trace_file, new_file);

    if (sortit) {
        lseek(new_file, 0, SEEK_SET);
        errs = sort_file(new_file, new_file);
    }

    close(new_file);
    close(trace_file);

    if (!errs)
        if (rename(new_file_name, trace_file_name) < 0)
            printf("Error renaming file: %d\n", errno);

    return 0;
}

int
process_trace_file(int trace_file, int new_file)
{
    ssize_t written;
    int errs = 0, match;
    int record_idx = 0;

    if (delete_job) printf("Deleting all jobs matched.\n");
    if (insert_job) printf("Inserting a new job after all jobs matched.\n");

    while (read(trace_file, &job_trace, sizeof(job_trace))) {
        ++record_idx;
        copy_to_new_val(); /* New record starts as copy of existing record. */
        if (select_idx == record_idx) {
            match = 1;
        } else if ( (select_idx < 0) &&
                    (rec_pat.job_id         == -1                            ||
                     job_trace.job_id     == rec_pat.job_id)               &&
                    (rec_pat.username[0]    == '\0'                          ||
                     !strcmp(job_trace.username,    rec_pat.username   ))  &&
                    (rec_pat.submit         == -1                            ||
                     job_trace.submit     == rec_pat.submit)               &&
                    (rec_pat.duration       == -1                            ||
                     job_trace.duration   == rec_pat.duration)             &&
                    (rec_pat.wclimit        == -1                            ||
                     job_trace.wclimit    == rec_pat.wclimit)              &&
                    (rec_pat.tasks          == -1                            ||
                     job_trace.tasks      == rec_pat.tasks)                &&
                    (rec_pat.qosname[0]     == '\0'                          ||
                     !strcmp(job_trace.qosname,     rec_pat.qosname    ))  &&
                    (rec_pat.partition[0]   == '\0'                          ||
                     !strcmp(job_trace.partition,   rec_pat.partition  ))  &&
                    (rec_pat.account[0]     == '\0'                          ||
                     !strcmp(job_trace.account,     rec_pat.account    ))  &&
                    (rec_pat.cpus_per_task  == -1                            ||
                     job_trace.cpus_per_task  == rec_pat.cpus_per_task)    &&
                    (rec_pat.tasks_per_node == -1                            ||
                     job_trace.tasks_per_node == rec_pat.tasks_per_node)   &&
                    (rec_pat.reservation[0] == '\0'                          ||
                     !strcmp(job_trace.reservation, rec_pat.reservation))  &&
                    (rec_pat.dependency[0]  == '\0'                          ||
                     !strcmp(job_trace.dependency,  rec_pat.dependency ))
                  ) {
            match = 1;
        } else
            match = 0;

        if ( !delete_job && match ) {
            fill_in_new_val();
            print_record("Existing Record: ", &job_trace);
            print_record("Update Record: ",   &upd_val);
            print_record("New Record:",       &new_val);
        }

        /* Now we write the "new" record if either of the following
         * conditions are true:
         * 1) We are not deleting jobs.
         * 2) We are deleting jobs and this job was NOT one that
         *    was matched.
         */
        if ( !delete_job || (delete_job && !match) ) {
            if (insert_job && match) {
                /* Write existing record first then the new record */
                written = write(new_file, &job_trace,
                                sizeof(job_trace));
                if (written <= 0) {
                    printf("Error! Zero bytes written.\n");
                    ++errs;
                }
            }
            written = write(new_file, &new_val, sizeof(new_val));
            if (written <= 0) {
                printf("Error! Zero bytes written.\n");
                ++errs;
            }
        }
    }

    return errs;
}

void
print_record(char* label, job_trace_t* jrec)
{
    printf("%s: %5d  \t%8s  \t%9s  \t%7s  \t%6s  \t%8ld  \t%8d  \t%7d  "
           "\t%5d(%d,%d)\n", label,
           jrec->job_id, jrec->username, jrec->partition, jrec->account,
           jrec->qosname, jrec->submit, jrec->duration, jrec->wclimit,
           jrec->tasks, jrec->tasks_per_node, jrec->cpus_per_task);
}

void
copy_to_new_val()
{
    /*
     * Copy existing record to the new record.
     * Individual fields will be updated if specified by
     * the user on the command line.
     */
    new_val.job_id              = job_trace.job_id;
    strcpy(new_val.username,      job_trace.username);
    new_val.submit              = job_trace.submit;
    new_val.duration            = job_trace.duration;
    new_val.wclimit             = job_trace.wclimit;
    new_val.tasks               = job_trace.tasks;
    strcpy(new_val.qosname,       job_trace.qosname);
    strcpy(new_val.partition,     job_trace.partition);
    strcpy(new_val.account,       job_trace.account);
    new_val.cpus_per_task       = job_trace.cpus_per_task;
    new_val.tasks_per_node      = job_trace.tasks_per_node;
    strcpy(new_val.reservation,   job_trace.reservation);
    strcpy(new_val.dependency,    job_trace.dependency);
}

void
strset(char* target, char* source)
{
    if (source[0] == ' ' )
        target[0] = '\0';
    else
        strcpy(target, source);
}

void
fill_in_new_val()
{
    /*
     * Individual fields will be updated if specified by
     * the user on the command line.
     */

    if (upd_val.job_id         != -1  )
        new_val.job_id            = upd_val.job_id;
    if (upd_val.username[0]    != '\0')
        strcpy(new_val.username,    upd_val.username   );
    if (upd_val.submit         != -1  ) {
        if (submit_time_type == ABSOLUTE) {
            new_val.submit            = upd_val.submit;
        } else if (submit_time_type == RELATIVE)
            new_val.submit            += upd_val.submit;
    }
    if (upd_val.duration       != -1  )
        new_val.duration          = upd_val.duration;
    if (upd_val.wclimit        != -1  )
        new_val.wclimit           = upd_val.wclimit;
    if (upd_val.tasks          != -1  )
        new_val.tasks             = upd_val.tasks;
    if (upd_val.qosname[0]     != '\0')
        strset(new_val.qosname,     upd_val.qosname    );
    if (upd_val.partition[0]   != '\0')
        strset(new_val.partition,   upd_val.partition  );
    if (upd_val.account[0]     != '\0')
        strset(new_val.account,     upd_val.account    );
    if (upd_val.cpus_per_task  != -1  )
        new_val.cpus_per_task     = upd_val.cpus_per_task;
    if (upd_val.tasks_per_node != -1  )
        new_val.tasks_per_node    = upd_val.tasks_per_node;
    if (upd_val.reservation[0] != '\0')
        strset(new_val.reservation, upd_val.reservation);
    if (upd_val.dependency[0]  != '\0')
        strset(new_val.dependency, upd_val.dependency);
}

void
init_record(job_trace_t*  jrec)
{
    jrec->job_id         = -1;
    jrec->username[0]    = '\0';
    jrec->submit         = -1;
    jrec->duration       = -1;
    jrec->wclimit        = -1;
    jrec->tasks          = -1;
    jrec->qosname[0]     = '\0';
    jrec->partition[0]   = '\0';
    jrec->account[0]     = '\0';
    jrec->cpus_per_task  = -1;
    jrec->tasks_per_node = -1;
    jrec->reservation[0] = '\0';
    jrec->dependency[0]  = '\0';
}

int
sort_file(int trace_file, int new_file)
{
    ssize_t      written;
    struct stat  stat_buf;
    int          nrecs = 0, errs = 0, idx = 0;
    job_trace_t* job_trace_head,* job_ptr,* job_arr,* job_cur;

    fstat(trace_file, &stat_buf);
    nrecs = stat_buf.st_size / sizeof(job_trace_t);

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*nrecs);
    if (!job_arr) {
        printf("Error.  Unable to allocate memory for all job records.\n");
        return -1;
    }

    read(trace_file, &job_arr[0], sizeof(job_trace));

    job_cur = job_trace_head = &job_arr[0];

    while (read(trace_file, &job_arr[++idx], sizeof(job_trace_t))) {

        job_arr[idx].next = NULL;

        if (job_arr[idx].submit < job_trace_head->submit) {
            job_arr[idx].next = job_trace_head;
            job_cur = job_trace_head = &job_arr[idx];
        } else {
            if (job_arr[idx].submit < job_cur->submit)
                job_ptr = job_trace_head;
            else
                job_ptr = job_cur;

            while (job_ptr->next && job_ptr->next->submit <=
                   job_arr[idx].submit)
                job_ptr = job_ptr->next;

            if (!job_ptr->next) {
                job_ptr->next = &job_arr[idx];
            } else {
                job_arr[idx].next = job_ptr->next;
                job_ptr->next = &job_arr[idx];
            }

            job_cur = &job_arr[idx];
        }
    }

    lseek(new_file, 0, SEEK_SET);

    job_ptr = job_trace_head;

    while (job_ptr) {
        written = write(new_file, job_ptr, sizeof(job_trace_t));
        if (written <= 0) {
            printf("Error! Zero bytes written.\n");
            ++errs;
        }

        job_ptr = job_ptr->next;
    }

    free (job_arr);

    return 0;
}
