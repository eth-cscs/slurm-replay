
#ifndef _LOGGER_H_
#define _LOGGER_H_
#include <stdarg.h>
#include <sys/stat.h>

#include "shmemclock.h"

FILE *logger = NULL;
static int daemon_flag = 1;

static void log_string(const char* type, char* msg)
{
    char log_time[32];
    time_t t = get_shmemclock();
    struct tm timestamp_tm;

    if (logger == NULL) {
        logger = stdout;
    }
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


void daemonize(int daemon_flag, const char* name)
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

        if (logger == NULL || logger == stdout) {
            logger = fopen(name, "w+");
        }
    } else {
        logger = stdout;
    }
}

#endif
