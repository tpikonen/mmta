/*
 * Runs 'send-queue' script of a given user.
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
#include <pwd.h>
#include <linux/limits.h>

#include "mmta-common.h"

#define SLEN 1024
#define DENY_ROOT 1


int main(int argc, char *argv[])
{
    struct passwd *userinfo;
    uid_t uid;
    char *uname;
    char shell[SLEN];
    char homedir[PATH_MAX];

    if(argc < 2) {
        fprintf(stderr, "mmta-send-queue: Not enough arguments.");
        exit(11); // bad arguments
    }
    uname = argv[1];
#if DENY_ROOT
    if(!strncmp(uname, "root", 5)) {
        exit(12); // privileged user not allowed
    }
#endif
    userinfo = getpwnam(uname);
    if(!userinfo) {
        exit(13); // bad <user>
    }
    uid = userinfo->pw_uid;
#if DENY_ROOT
    if(uid < 1000) {
        exit(12); // privileged user not allowed
    }
#else
    if((uid < 1000) && (uid != 0)) {
        exit(12); // privileged user not allowed
    }
#endif
    strncpy(shell, userinfo->pw_shell, SLEN-1);
    shell[SLEN-1] = '\0';
    strncpy(homedir, userinfo->pw_dir, PATH_MAX-1);
    homedir[PATH_MAX-1] = '\0';
    /* Check if user is ok to receive mail */
    if(!checkshell(shell)) {
        fprintf(stderr, "mmta-send-queue: Receiver %s not allowed to log in, exiting.", \
            uname);
        exit(13); // bad <user>
    }

    /* Drop privs */
    if(setuid(uid) != 0) {
        perror("setuid failed");
        exit(14); // could not drop privilegdes
    }

    char *sargv[SLEN];
    char script[PATH_MAX];
    int status;

    if(find_script(script, "send-queue", homedir)) {
        sargv[0] = script;
        sargv[1] = NULL;
        status = runprog(sargv, stdin, homedir);
        if(WIFEXITED(status)) {
            exit(WEXITSTATUS(status));
        }
        exit(17); // script did not exit normally
    }
    exit(16); // script not found
}
