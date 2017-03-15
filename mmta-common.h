/*
 * Function definitions from mmta-common.c
 * Part of mmta, a minimal mail tansport agent
 * Copyright: 2008-2017 Teemu Ikonen <tpikonen@gmail.com>
 * License: GPLv2+
 */

#define SLEN 1024

#define debug_print(FORMAT, ...) \
    do { if (DEBUG) fprintf(stderr, "%s() in %s:%i: " FORMAT "\n", \
        __func__, __FILE__, __LINE__, ##__VA_ARGS__); } while (0)

int checkshell(const char *shell);
char *find_script(char *fullpath, const char *script, const char *homedir);
void execprog(char * const argv[], const char *homedir);
int runprog(char * const argv[], FILE *input, const char *homedir);
