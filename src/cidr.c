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
#ifdef __linux__
#include <linux/tcp.h>
#endif

#include "ctunnel.h"

int in_subnet(char *net, char *host, char *mask)
{
    unsigned long n, h, m = (unsigned long)-0;

    if ((n = inet_addr(net)) == INADDR_NONE)
        return -1;
    if ((h = inet_addr(host)) == INADDR_NONE)
        return -1;
    if ((m = inet_addr(mask)) == INADDR_NONE)
        return -1;
    n = ntohl(n);
    h = ntohl(h);
    m = ntohl(m);

    return ((n & m) == (h & m));
}
int cidr_to_mask(char *data, int cidr)
{
    int order = 32 / cidr;
    int msk;
    long pow = 1;
    int i = 0;

    if (cidr < 1 || cidr > 32)
        return -1;

    if (cidr > 0)
        order = 1;
    if (cidr > 8)
        order = 2;
    if (cidr > 16)
        order = 3;
    if (cidr > 24)
        order = 4;

    for (i = 0; i < order; i++)
    {
        pow = 256 * pow;
    }
    //    pow -= 2;
    for (i = 1; i < order; i++)
    {
        strncat(data, "255.", 4);
    }
    msk = 256 - (pow >> cidr);
    sprintf(data, "%s%d", data, msk);
    for (i = 0; i < 4 - order; i++)
        strncat(data, ".0", 2);

    return 0;
}
int main(int argc, char *argv[])
{
    char nm[256] = "";
    cidr_to_mask(nm, atoi(argv[1]));
    fprintf(stdout, "[%s]\n", nm);
}
