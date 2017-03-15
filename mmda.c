/*
 * mmda, a minimal mail delivery agent
 * Copyright: 2008-2017 Teemu Ikonen <tpikonen@gmail.com>
 * License: GPLv2+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <lockfile.h>
#include <time.h>
#include <linux/limits.h>

#include "mmta-common.h"

#define DENY_ROOT 1
#define MAILDIR "/var/mail"
/* The GID for group mail is always 8 at least in Debian */
#define MAIL_GID (8)


/* Make sure a correct mbox file for user exists */
/* This function is run as suid root */
void touchmbox(const char *uname, int uid)
{
    char mboxname[PATH_MAX];
    struct stat ss;

    snprintf(mboxname, PATH_MAX-1, "%s/%s", MAILDIR, uname);
    if(stat(mboxname, &ss) < 0) {
        int fd, ret;
        /* touch file */
        if((fd = creat(mboxname, 0660)) < 0) {
            perror("Could not create mailbox");
            exit(1);
        }
        if((ret = fchown(fd, uid, MAIL_GID)) < 0) {
            perror("Could not fchown mailbox");
            exit(1);
        }
        close(fd);
        stat(mboxname, &ss);
    }
    /* Check that the file is owned by the user and has correct perms */
    if(ss.st_uid != uid || !S_ISREG(ss.st_mode) || (ss.st_mode & 0707) != 0600 )
    {
        fprintf(stderr, "Unacceptable owner or mode on mailbox %s", mboxname);
        exit(1);
    }
}


int mboxmail(FILE* infile, const char *mboxname, const char *callername)
{
    FILE *f;
    char lockname[PATH_MAX];
    int locksz;
    int c;
    time_t t;

    locksz = snprintf(lockname, PATH_MAX-1, "%s.lock", mboxname);
    if((locksz != strnlen(mboxname, PATH_MAX)+5) \
        || lockfile_create(lockname, 4, 0) != 0) {
        return 13;
    }
    f = fopen(mboxname, "a");
    if(f == NULL) {
        /* Could not create mbox? */
        lockfile_remove(lockname);
        return 3;
    }
    time(&t);
    //FIXME: doesn't print timezone
    fprintf(f, "From %s %s", callername, ctime(&t));
    c = getc(infile);
    while(!feof(infile)) {
        putc(c, f);
        c = getc(infile);
    }
    fprintf(f, "\n");
    if(lockfile_remove(lockname) != 0) {
        return 1;
    }
    if(ferror(f)) {
        /* some error */
        return 1;
    }
    fclose(f);
    return 0;
}


int usermboxmail(FILE* infile, const char *uname, const char* callername) {
    char mboxname[PATH_MAX];

    snprintf(mboxname, PATH_MAX, "%s/%s", MAILDIR, uname);
    return mboxmail(infile, mboxname, callername);
}


void eat_wspace(char *buf)
{
    int i;

    i = 0;
    while(buf[i] == ' ' || buf[i] == '\t')
        i++;
    if(i > 0) {
        memmove(buf, buf+i, SLEN-i);
    }
}


void runforward(const char *fwdname, FILE *mfile, const char *uname,
                const char *homedir, char *callername, int send,
                int force_mbox)
{
    FILE *fwd;
    char buf[SLEN];
    int blen;
    int status;
    char *sargv[SLEN];
    int sargc;
    char scriptname[PATH_MAX];
    int mail_is_mboxed;

    fwd = fopen(fwdname, "r");
    if(fwd == NULL) {
        fprintf(stderr, "mmda: Forward file %s disappeared?\n", fwdname);
        exit(1);
    }

    mail_is_mboxed = 0;
    sargc = 1;
    while(!feof(fwd)) {
        fseek(mfile, 0, SEEK_SET);
        if(fgets(buf, SLEN, fwd) == NULL) {
            break;
        }
        blen = strnlen(buf, SLEN);
        if(buf[blen-1] == '\n') {
            buf[blen-1] = '\0';
        }
        debug_print("The .forward line: |%s|", buf);
        eat_wspace(buf);
        /* Ignore comment lines */
        if(buf[0] == '#') {
            debug_print("   is a comment.");
            continue;
        }
        /* Simple unquoting */
        if(buf[0] == '"' || buf[0] == '\'') {
            char *end;

            end = strrchr(buf, buf[0]);
            if(end <= buf) {
                exit(0);
            }
            memmove(buf, buf+1, end-buf-1);
            buf[end-buf-1] = '\0';
            debug_print("   after unquoting: |%s|", buf);
        }
        eat_wspace(buf);
        if(buf[0] == '\\') {
            memmove(buf, buf+1, SLEN-1);
            debug_print("   after backslash removal: |%s|", buf);
        }
        if(buf[0] == '|') {
            char *argv[SLEN];
            char *tok;
            int i;

            i = 0;
            tok = strtok(buf+1, " \n");
            argv[0] = tok;
            while(tok != NULL) {
                i++;
                tok = strtok(NULL, " \n");
                argv[i] = tok;
            }
            debug_print("   is pipe to |%s|", argv[0]);
            runprog(argv, mfile, homedir);
        } else if(buf[0] == '/') {
            mboxmail(mfile, buf, callername);
            debug_print("   is extra mbox file |%s|", buf);
        } else if(strchr(buf, '@')) {
            debug_print("   is external address:|%s|", buf);
            if(send) {
                sargv[sargc] = (char *) malloc(strnlen(buf, SLEN)+1);
                if(sargv[sargc] == NULL) {
                    fprintf(stderr, "mmda: malloc failed.");
                    exit(1);
                }
                strcpy(sargv[sargc], buf);
                sargc++;
            } else {
                printf("%s\n", buf);
            }
        } else if(strncmp(uname, buf, SLEN) == 0) {
                usermboxmail(mfile, uname, callername); // FIXME: check retval
                mail_is_mboxed = 1;
                debug_print("   is own mailbox of user %s", uname);
        } else {
            debug_print("   is local address:|%s|", buf);
            printf("%s\n", buf);
        }
    }
    fclose(fwd);

    if(send && find_script(scriptname, "queue-mail", homedir)) {
        sargv[0] = scriptname;
        sargv[sargc] = NULL;
        status = runprog(sargv, mfile, homedir);
        if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            exit(66);
        }
        // FIXME: Maybe run free() on sargv[>0]
    }
    if(force_mbox && !mail_is_mboxed) {
        usermboxmail(mfile, uname, callername); // FIXME: check retval
        debug_print("Forced mboxing.");
    }
    exit(0);
}


int main(int argc, char *argv[])
{
    struct passwd *userinfo;
    uid_t uid;
    char *uname;
    char callername[SLEN];
    char shell[SLEN];
    char homedir[PATH_MAX];
    char fwdname[PATH_MAX];
    int no_external, force_mbox;
    int i;

    if(argc < 2) {
        fprintf(stderr, "mmda: Not enough arguments.");
        exit(1);
    }
    no_external = 0;
    force_mbox = 0;
    for(i = 1; i < argc && argv[i][0] == '-'; i++) {
        if(strncmp(argv[i], "--no-external", 13) == 0) {
            no_external = 1;
        } else if(strncmp(argv[i], "--force-mbox", 12) == 0) {
            force_mbox = 1;
        } else {
            fprintf(stderr, "mmda: Unknown option '%s'\n in command line.\n", \
                argv[i]);
            exit(4);
        }
    }
    if(i >= argc) {
        fprintf(stderr, "mmda: No <user> argument found.\n");
        exit(4);
    }
    uname = argv[i];
#if DENY_ROOT
    if(!strncmp(uname, "root", 5)) {
        exit(2);
    }
#endif
    userinfo = getpwnam(uname);
    if(!userinfo) {
        exit(3);
    }
    uid = userinfo->pw_uid;
#if DENY_ROOT
    if(uid < 1000) {
#else
    if((uid < 1000) && (uid != 0)) {
#endif
        exit(2);
    }
    strncpy(shell, userinfo->pw_shell, SLEN-1);
    shell[SLEN-1] = '\0';
    strncpy(homedir, userinfo->pw_dir, PATH_MAX-1);
    homedir[PATH_MAX-1] = '\0';
    /* Check if user is ok to receive mail */
    if(!checkshell(shell)) {
        fprintf(stderr, "mmda: Receiver %s not allowed to log in, exiting.", \
            uname);
        exit(1);
    }
    touchmbox(uname, uid);

    do {
        struct passwd *callerinfo;
        uid_t cuid;

        cuid = getuid();
        callerinfo = getpwuid(cuid);
        if(!callerinfo) {
            exit(3);
        }
        strncpy(callername, callerinfo->pw_name, SLEN-1);
    } while(0);

    /* Drop privs */
    if(setuid(uid) != 0) {
        perror("setuid failed");
        exit(4);
    }

    snprintf(fwdname, PATH_MAX-1, "%s/.forward", homedir);
    do {
        int statret;
        struct stat ss;

        statret = stat(fwdname, &ss);
        if((statret != 0) || ((ss.st_uid != uid) && (ss.st_uid != 0)) \
            || !S_ISREG(ss.st_mode) || ss.st_mode & (S_IWGRP | S_IWOTH)) {
            /* .forward does not exist or not owned by user or root
             * or is not regular file or is group or world writable.
             * */
            // FIXME: do error handling on mboxmail and return
            // a known exit code for invalid .forward
            return usermboxmail(stdin, uname, callername);
        }
    } while(0);

    do {
        int send, status, c;
        FILE *mfile;

        send = 0;
        if(!no_external) {
            char *sargv[SLEN];
            char script[PATH_MAX];

            /* Check if we can send external mail */
            if(find_script(script, "can-send", homedir)) {
                sargv[0] = script;
                sargv[1] = NULL;
                status = runprog(sargv, stdin, homedir);
                if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    send = 1;
                }
            }
        }
        mfile = tmpfile();
        if(mfile == NULL) {
            fprintf(stderr, "mmda: Could not open temporary file.\n");
            exit(1);
        }
        c = getc(stdin);
        while(!feof(stdin)) {
            putc(c, mfile);
            c = getc(stdin);
        }
        if(ferror(mfile)) {
            /* some error */
            fprintf(stderr, "mmda: Error copying to temporary file.\n");
            exit(1);
        }
        runforward(fwdname, mfile, uname, homedir, callername, send, \
            force_mbox);
    } while(0);
    exit(0);
}
