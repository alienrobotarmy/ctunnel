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
#include <stdarg.h>

#include "ctunnel.h"

void ctunnel_log(FILE *fp, int facility, const char *format, ...)
{
    va_list args;
    char text[2048];

    va_start(args, format);
    vsprintf(text, format, args);
    va_end(args);


#ifndef _WIN32
    syslog(facility, "%s%s", (facility == LOG_CRIT) ? "ERROR: " : "", text);
#endif
    fprintf(fp, "[ctunnel] %s%s\n", (facility == LOG_CRIT) ? "ERROR: " : "", text);
}
