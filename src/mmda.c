/* 
 * mmda, a minimal mail delivery agent
 * Copyright: 2008 Teemu Ikonen <tpikonen@gmail.com>
 * License: GPLv2+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#define SLEN 1024
#define BUFSIZE 4096
#define DENY_ROOT 1
#define MAILDIR "/var/mail"
#define MAIL_GID (8)


/* Check if a given shell is in /etc/shells */
void checkshell(const char *shell)
{
    char *p;

    while((p = getusershell()) != NULL) {
        if(!strcmp(p, shell)) {
            endusershell();
            return;
        }
    }
    exit(1);
}


/* Make sure a correct mbox file for user exists */
/* This function is run as suid root */
void touchmbox(const char *uname, int uid)
{
    char mbox[SLEN];
    struct stat ss;    

    strcpy(mbox, MAILDIR);
    strncat(mbox, "/", SLEN-1);
    strncat(mbox, uname, SLEN-1);
    mbox[SLEN-1] = '\0';
    if(stat(mbox, &ss) < 0) {
        int fd;
        /* touch file */
        if((fd = creat(mbox, 0660)) < 0) {
            exit(1);
        }
        fchown(fd, uid, MAIL_GID);
        close(fd);
        stat(mbox, &ss);
    }
    /* Check that the file is owned by the user and has correct perms */
    if(ss.st_uid != uid || ss.st_gid != MAIL_GID || !S_ISREG(ss.st_mode)
            || (ss.st_mode & 0777) != 0660 )
    {
        exit(1);
    } 
}


void catmail(const char *uname)
{
    char mbox[SLEN];
    int ulen;

    ulen = strlen(uname);
    strcpy(mbox, MAILDIR);
    strncat(mbox, uname, SLEN-1);
    mbox[SLEN-1] = '\0';
    if(access(mbox, F_OK)) {
        
    }
}


void catforward(const char *homedir)
{
    char fwdfile[SLEN];
    int fd, nread, nwritten;
    char *buf[BUFSIZE];

    strncpy(fwdfile, homedir, SLEN-10);
    strncat(fwdfile, "/.forward", 10); 
    fd = open(fwdfile, O_RDONLY);
    if(fd < 3) {
        exit(1);
    }
    while((nread = read(fd, buf, BUFSIZE)) > 0) {
        nwritten = 0;
        while(nwritten < nread) {
            int nout;
            nout = write(1, buf, nread-nwritten);
            if(nout < 0) /* write error */
                exit(1);
            nwritten += nout;
        }
    }
    if(nread < 0) { /* read error */
        exit(1);
    }
}


void runforward(const char *homedir)
{

}


void cansend(const char *homedir)
{

}


void queuemail(const char *uname)
{

}


void runqueue(const char *uname)
{

}


int main(int argc, char *argv[])
{
    struct passwd *userinfo;
    uid_t uid;
    gid_t gid;
    char *uname;
    char *cmd;
    char shell[SLEN];
    char homedir[SLEN];


    if(argc < 3) {
        exit(1);
    }
    printf("aimo1\n");
    uname = argv[2];
#if DENY_ROOT
    if(!strncmp(uname, "root", 5)) {
        exit(2);
    }
#endif
    printf("aimo2\n");
    userinfo = getpwnam(uname);
    if(!userinfo) {
        exit(3);
    }
    printf("aimo3\n");
    uid = userinfo->pw_uid;
#if DENY_ROOT
    if(uid < 1000) {
#else
    if((uid < 1000) && (uid != 0)) {
#endif
        exit(2);
    }
    printf("aimo4\n");
    gid = userinfo->pw_gid;
    strncpy(shell, userinfo->pw_shell, SLEN-1);
    shell[SLEN-1] = '\0';
    strncpy(homedir, userinfo->pw_dir, SLEN-1);
    homedir[SLEN-1] = '\0';
    /* destroy remaining user information */
    userinfo = getpwnam("");
    /* Check if user is ok to receive mail */
    checkshell(shell);
    printf("aimo5\n");
    /* Make sure user has a mailbox */
    touchmbox(uname, uid);
    printf("aimo6\n");
    /* Give up privs */
    if(setuid(uid) != 0) {
        exit(4);
    }
    printf("aimo7\n");
    cmd = argv[1];
    if(!strncmp(cmd, "cat-mail", 9)) {
        catmail(uname);
    } else if(!strncmp(cmd, "cat-forward", 12)) {
        catforward(homedir);
    } else if(!strncmp(cmd, "run-forward", 12)) {
        if(argc < 4) {
            exit(1);
        }
        runforward(homedir);
    } else if(!strncmp(cmd, "can-send", 9)) {
        cansend(homedir);
    } else if(!strncmp(cmd, "queue-mail", 11)) {
        queuemail(uname);
    } else if(!strncmp(cmd, "run-queue", 10)) {
        runqueue(uname);
    }
}
