/*
 * Ctunnel - Cyptographic tunnel.
 * Copyright (C) 2008-2020 Jess Mahan <ctunnel@alienrobotarmy.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "ctunnel.h"

int keyfile_read(struct options *opt, char *filename)
{
    char *homedir, *passkey;
    struct stat st;
    int fd;

    if (!filename)
    {
#ifndef _WIN32
        homedir = getenv("HOME");
        passkey = calloc(1, strlen(homedir) + strlen(KEYFILE) + 2);
        snprintf(passkey, strlen(homedir) + strlen(KEYFILE) + 1, "%s%s",
                 homedir, KEYFILE);
#else
        homedir = getenv("APPDATA");
        passkey = calloc(1, sizeof(char *) * 25);
        snprintf(passkey, strlen(homedir) + 25, "%s\\ctunnel-passkey",
                 homedir);
#endif
    }
    else
    {
        passkey = strdup(filename);
    }
    if (stat(passkey, &st) < 0)
    {
        return 0;
        //    fprintf(stdout, "No Passkey, will prompt\n");
    }
#ifndef _WIN32
    fd = open(passkey, O_RDONLY,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
#else
    fd = _open(passkey, O_RDONLY | O_BINARY);
#endif
    if (fd < 0)
    {
        free(passkey);
        return fd;
    }
    if (read(fd, &opt->key, sizeof(struct keys)) == -1)
    {
        ctunnel_log(stderr, LOG_CRIT, "pipe(): %s", strerror(errno));
        return -1;
    }
    close(fd);
    free(passkey);
    return 1;
    /*
	strcpy(opt->cipher, opt->key.ciphername);
        if (opt->key.version != CONFIG_VERSION) {
            fprintf(stderr, "Passkey in %s not compatible, please delete "
                    "and re-reun ctunnel\n", passkey);
            exit(1);
        }
        ctunnel_log(stdout, LOG_INFO,
                    "Found Stored Key and Cipher[%s] in %s",
                    opt->key.ciphername, passkey);
        close(fd);
    }
    free(passkey);
*/
}
int keyfile_write(struct options *opt, char *filename)
{
    char *homedir, *passkey;
    int fd;

    if (!filename)
    {
#ifndef _WIN32
        homedir = getenv("HOME");
        passkey = calloc(1, strlen(homedir) + strlen(KEYFILE) + 2);
        snprintf(passkey, strlen(homedir) + strlen(KEYFILE) + 1, "%s%s",
                 homedir, KEYFILE);
#else
        homedir = getenv("APPDATA");
        passkey = calloc(1, sizeof(char *) * 25);
        snprintf(passkey, strlen(homedir) + 25, "%s\\ctunnel-passkey",
                 homedir);
#endif
    }
    else
    {
        passkey = strdup(filename);
    }
#ifdef _WIN32
    fd = open(passkey, O_RDWR | O_BINARY | O_CREAT);
#else
    fd = open(passkey, O_RDWR | O_TRUNC | O_CREAT,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
#endif
    if (fd < 0)
    {
        free(passkey);
        return fd;
    }
    opt->key.version = CONFIG_VERSION;
    if (write(fd, &opt->key, sizeof(struct keys)) == -1)
    {
        ctunnel_log(stderr, LOG_CRIT, "pipe(): %s", strerror(errno));
        return -1;
    }
    close(fd);
    free(passkey);
    return 1;
}
