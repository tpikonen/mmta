/*
 * Common functions for mmda and mmta-send-queue.
 * Part of mmta, a minimal mail tansport agent
 * Copyright: 2008-2017 Teemu Ikonen <tpikonen@gmail.com>
 * License: GPLv2+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/limits.h>

#include "mmta-common.h"

#define SLEN 1024


/* Check if a given shell is in /etc/shells
 * Return 1 if it is, 0 if not */
int checkshell(const char *shell)
{
    char *p;

    while((p = getusershell()) != NULL) {
        if(!strcmp(p, shell)) {
            endusershell();
            return 1;
        }
    }
    return 0;
}


char *find_script(char *fullpath, const char *script, const char *homedir)
{
    struct stat ss;

    snprintf(fullpath, PATH_MAX, "%s/%s/%s", homedir, USERCONFDIR, script);
    if(!stat(fullpath, &ss) && (ss.st_mode & S_IXUSR)) {
        return fullpath;
    } else {
        snprintf(fullpath, PATH_MAX, "%s/%s", SYSCONFDIR, script);
        if(!stat(fullpath, &ss) && (ss.st_mode & S_IXUSR)) {
            return fullpath;
        } else {
            return NULL;
        }
    }
}


void execprog(char * const argv[], const char *homedir)
{
    char *newenv[3];
    char *path = "PATH=/bin:/usr/bin";
    char home[PATH_MAX];

    snprintf(home, PATH_MAX, "HOME=%s", homedir);
    newenv[0] = path;
    newenv[1] = home;
    newenv[2] = NULL;

    execve(argv[0], argv, newenv);
    exit(1);
}


int runprog(char * const argv[], FILE *input, const char *homedir)
{
    int status;
    pid_t cpid;

    if((cpid = fork()) < 0) {
        perror("fork failed");
        exit(123);
    }
    if(cpid == 0) {
        dup2(fileno(input), 0);
        execprog(argv, homedir);
    }
    wait(&status);

    return status;
}
