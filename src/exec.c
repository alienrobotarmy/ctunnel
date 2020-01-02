/*
 * Ctunnel - Cyptographic tunnel.
 * Copyright (C) 2008-2020 Jess Mahan <ctunnel@alienrobotarmy.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#ifdef __linux__
#include <pty.h>
#endif
#ifdef __APPLE__
#include <util.h>
#endif

#include "ctunnel.h"

#define R 0
#define W 1

int run(char *f)
{
#ifndef _WIN32
    pid_t pid;
    int i, ret;
    int sout[2];
    char data[1024];
    struct JToken *tok;
    struct stat st;
    char **args;
    extern char **environ;
    char ch;

    if ((stat(f, &st)) < 0)
    {
        ctunnel_log(stderr, LOG_CRIT, "%s: %s", f, strerror(errno));
        return -1;
    }
    args = calloc(1, sizeof(char **));
    tok = calloc(1, sizeof(struct JToken));
    tok->so = 0;
    tok->eo = 0;
    tok->match = NULL;
    for (i = 0; (ret = jtok(tok, f, ' ')) > 0; i++)
    {
        args = realloc(args, sizeof(char **) * (i + 1));
        args[i] = strdup(tok->match);
    }
    free(tok->match);
    free(tok);
    args = realloc(args, sizeof(char **) * (i + 1));
    args[i] = NULL;

    pipe(sout);

    if ((pid = fork()) == 0)
    {
        /*
        if (ct->net_cli->sockfd > 3)
            close(ct->net_cli->sockfd);
        if (ct->net_srv->sockfd > 3)
            close(ct->net_srv->sockfd);
    */
        setsid();
        dup2(sout[W], fileno(stdout));
        dup2(sout[W], fileno(stderr));
        close(sout[R]);
        execve(args[0], args, environ);
    }
    else
    {
        close(sout[W]);
        while (read(sout[R], &ch, sizeof(char)))
        {
            if (ch != '\n')
            {
                data[i++] = ch;
            }
            else
            {
                ctunnel_log(stdout, LOG_INFO, "[exec] %s", data);
                i = 0;
                memset(data, 0, sizeof(data));
            }
        }
        waitpid(pid, &ret, WUNTRACED);
        close(sout[R]);
    }
    for (i = 0; args[i]; i++)
        free(args[i]);
    free(args);

    return WEXITSTATUS(ret);
#else
    return 0;
#endif
}
int prun(char *f)
{
    int p = 0;
#ifndef _WIN32
    pid_t pid;
    int i, ret;
    int so[2], si[2];
    struct JToken *tok;
    struct stat st;
    char **args;
    extern char **environ;

    ctunnel_log(stdout, LOG_INFO, "running: %s", f);

    args = calloc(1, sizeof(char **));
    tok = calloc(1, sizeof(struct JToken));
    tok->so = 0;
    tok->eo = 0;
    tok->match = NULL;
    for (i = 0; (ret = jtok(tok, f, ' ')) > 0; i++)
    {
        args = realloc(args, sizeof(char **) * (i + 1));
        args[i] = strdup(tok->match);
    }
    free(tok->match);
    free(tok);
    args = realloc(args, sizeof(char **) * (i + 1));
    args[i] = NULL;

    if ((stat(args[0], &st)) < 0)
    {
        ctunnel_log(stderr, LOG_CRIT, "prun %s: %s", f, strerror(errno));
        return -1;
    }

    pipe(so);
    pipe(si);

    if ((pid = forkpty(&p, NULL, NULL, NULL)) == 0)
    {
        execve(args[0], args, environ);
    }
    for (i = 0; args[i]; i++)
        free(args[i]);
    free(args);
#endif
    return p;
}
