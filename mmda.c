/* 
 * mmda, a minimal mail delivery agent
 * Copyright: 2008 Teemu Ikonen <tpikonen@gmail.com>
 * License: GPLv2+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <lockfile.h>

#define SLEN 1024
#define DENY_ROOT 1
#define MAILDIR "/var/mail"
#define QEXT ".mailq"
#define SAFECAT "/usr/bin/safecat"
#define MAIL_GID (8)

#define SCRIPTDIR "/home/tpikonen/mailscript/git-mailscript/msmtp"

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


void execprog(char * const argv[], const char *homedir)
{
    char *newenv[3];
    char *path = "PATH=/bin:/usr/bin";
    char home[SLEN];
    int i;

    snprintf(home, SLEN, "HOME=%s", homedir);
    newenv[0] = path;
    newenv[1] = home;
    newenv[2] = NULL;

    i = 0;
    while(argv[i] != NULL) {
        printf("argv%d: %s\n", i, argv[i++]);
    }

    execve(argv[0], argv, newenv);
    printf("error: returning from exec\n");
    exit(1);
}


/* Make sure a correct mbox file for user exists */
/* This function is run as suid root */
void touchmbox(const char *uname, int uid)
{
    char mboxname[SLEN];
    struct stat ss;

    snprintf(mboxname, SLEN-1, "%s/%s", MAILDIR, uname);
    if(stat(mboxname, &ss) < 0) {
        int fd;
        /* touch file */
        if((fd = creat(mboxname, 0660)) < 0) {
            exit(1);
        }
        fchown(fd, uid, MAIL_GID);
        close(fd);
        stat(mboxname, &ss);
    }
    /* Check that the file is owned by the user and has correct perms */
    if(ss.st_uid != uid || ss.st_gid != MAIL_GID || !S_ISREG(ss.st_mode)
            || (ss.st_mode & 0777) != 0660 )
    {
        exit(1);
    }
}


int mboxmail(FILE* infile, const char *mboxname, const char *cname)
{
    FILE *f;
    char lockname[SLEN];
    int locksz;
    char *argv[2];
    int c;
    time_t t;

    locksz = snprintf(lockname, SLEN-1, "%s.lock", mboxname);
    if((locksz != strlen(mboxname)+5) || lockfile_create(lockname, 4, 0) != 0) {
        return 13;
    }
    f = fopen(mboxname, "a");
    if(f == NULL) {
        /* Could not create mbox? */
        lockfile_remove(lockname); 
        return 3;
    }
    time(&t);
    fprintf(f, "From %s %s\n", cname, ctime(&t)); //FIXME: doesn't print timezone
    c = getc(infile);
    while(!feof(infile)) {
        putc(c, f);
        c = getc(infile);
    }
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


void maildirmail(const char *uname, int uid, const char *homedir)
{
    char *safecat = SAFECAT;
    char mdirtmp[SLEN];
    char mdirnew[SLEN];
    char *argv[4];
    struct stat ss;

    /* Check maildir components and perms */
    snprintf(mdirtmp, SLEN-1, "%s/%s%s/%s", MAILDIR, uname, QEXT, "tmp");
    if(stat(mdirtmp, &ss) < 0 || ss.st_uid != uid /* || ss.st_gid != MAIL_GID */
            || !S_ISDIR(ss.st_mode) /* || (ss.st_mode & 0777) != 0660 */ )
    {
        exit(1);
    }
    snprintf(mdirnew, SLEN-1, "%s/%s%s/%s", MAILDIR, uname, QEXT, "new");
    if(stat(mdirnew, &ss) < 0 || ss.st_uid != uid /* || ss.st_gid != MAIL_GID */
            || !S_ISDIR(ss.st_mode) /* || (ss.st_mode & 0777) != 0660 */ )
    {
        exit(1);
    }
    argv[0] = safecat;
    argv[1] = mdirtmp;
    argv[2] = mdirnew;
    argv[3] = NULL;
    execprog(argv, uname, homedir);
}


/* Output users' ~/.forward file to stdout
 * Return values:
 * 0 if file exists, is owned by user or root and can be read 
 * 1 if the file does not exist, or is not owned by user or root
 * 2 if there's some other error
 */
void catforward(int uid, const char *homedir)
{
    char fwdname[SLEN];
    int fd, nread, nwritten;
    char *buf[SLEN];
    struct stat ss;
    int statret;

    strncpy(fwdname, homedir, SLEN-10);
    strncat(fwdname, "/.forward", 10);
    statret = stat(fwdname, &ss);
    if((statret != 0) || ((ss.st_uid != uid) && (ss.st_uid != 0))) {
        /* .forward does not exist or is not owned by user or root */
        exit(1);
    }

    fd = open(fwdname, O_RDONLY);
    if(fd < 3) {
        exit(2);
    }
    while((nread = read(fd, buf, SLEN)) > 0) {
        nwritten = 0;
        while(nwritten < nread) {
            int nout;
            nout = write(1, buf, nread-nwritten);
            if(nout < 0) /* write error */
                exit(2);
            nwritten += nout;
        }
    }
    if(nread < 0) { /* read error */
        exit(2);
    }
    exit(0);
}


void runforward(int uid, const char *uname, const char *homedir, unsigned int lineno)
{
    FILE *fwd;
    char fwdname[SLEN];
    char buf[BUFSIZE];
    unsigned int i;
    struct stat ss;
    int statret;

    strncpy(fwdname, homedir, SLEN-10);
    strncat(fwdname, "/.forward", 10);
    statret = stat(fwdname, &ss);
    if((statret != 0) || ((ss.st_uid != uid) && (ss.st_uid != 0))) {
        /* .forward does not exist or not owned by user or root */
        exit(0);
    }

    fwd = fopen(fwdname, "r");
    if(fwd == NULL) {
        exit(1);
    }
    i = 0;
    while((i < lineno) && !feof(fwd)) {
        fgets(buf, BUFSIZE, fwd);
        i++;
    }
    if(feof(fwd) && (i <= lineno)) {
        exit(2);
    }
    fclose(fwd);
    printf("buf: |%s|\n", buf);
    /* Ignore comment lines */
    if(buf[0] == '#') {
        exit(3);
    }
    /* Simple unquoting */
    if(buf[0] == '"' || buf[0] == '\'') {
        char *end;

        end = strrchr(buf, buf[0]);
        if(end <= buf) {
            exit(0);
        }
        strncpy(buf, buf+1, end-buf-1);
        buf[end-buf-1] = '\0';
    }
    printf("unquoted: |%s|\n", buf);
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
        execprog(argv, uname, homedir);
    } else if(buf[0] == '/') {

    }
    printf("Unknown .forward file command\n");
    exit(1);
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
    struct passwd *callerinfo;
    uid_t uid, cuid;
    gid_t gid;
    char *uname;
    char *cmd;
    char cname[SLEN];
    char shell[SLEN];
    char homedir[SLEN];


    if(argc < 3) {
        exit(1);
    }
    uname = argv[1];
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
    gid = userinfo->pw_gid;
    strncpy(shell, userinfo->pw_shell, SLEN-1);
    shell[SLEN-1] = '\0';
    strncpy(homedir, userinfo->pw_dir, SLEN-1);
    homedir[SLEN-1] = '\0';
    /* Check if user is ok to receive mail */
    checkshell(shell);

    cuid = getuid();
    callerinfo = getpwuid(cuid);
    if(!callerinfo) {
        exit(3);
    }
    strncpy(cname, callerinfo->pw_name, SLEN-1);

    /* run a command */
    /* if an mbox for user does not exist, it needs to
     * be created with root privs
     */
    cmd = argv[2];
    if(!strncmp(cmd, "mbox", 5)) {
        touchmbox(uname, uid);
        if(setuid(uid) != 0) {
            exit(4);
        }
        mboxmail(uname, cname);
        exit(0);
    }
    /* rest of the commands do not need privileges */
    if(setuid(uid) != 0) {
        exit(4);
    }
    if(!strncmp(cmd, "maildir", 12)) {
        maildirmail(uname, uid, homedir);
    } else if(!strncmp(cmd, "cat-forward", 12)) {
        catforward(uid, homedir);
    } else if(!strncmp(cmd, "run-forward", 12)) {
        unsigned int lineno;

        if(argc < 4) {
            exit(1);
        }
        lineno = 0;
        sscanf(argv[3], "%5u", &lineno);
        runforward(uid, uname, homedir, lineno);
    } else if(!strncmp(cmd, "can-send", 9)) {
        cansend(homedir);
    } else if(!strncmp(cmd, "queue-mail", 11)) {
        queuemail(uname);
    } else if(!strncmp(cmd, "run-queue", 10)) {
        runqueue(uname);
    }

    printf("unknown command\n");
    return 1;
}
