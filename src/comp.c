/*
 * Ctunnel - Cyptographic tunnel.
 * Copyright (C) 2008-2014 Jess Mahan <ctunnel@nardcore.org>
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

struct Compress z_compress_init(struct options opt)
{
    struct Compress comp;
    int ret = 0;

    comp.deflate.zalloc = Z_NULL;
    comp.deflate.zfree = Z_NULL;
    comp.deflate.opaque = Z_NULL;
    ret = deflateInit(&comp.deflate, opt.comp_level);
    if (ret != Z_OK) {
	ctunnel_log(stderr, LOG_CRIT, "Error Initializing Zlib (deflate)");
	exit(1);
    }

    comp.inflate.zalloc = Z_NULL;
    comp.inflate.zfree = Z_NULL;
    comp.inflate.opaque = Z_NULL;
    comp.inflate.avail_in = 0;
    comp.inflate.next_in = Z_NULL;
    ret = inflateInit(&comp.inflate);
    if (ret != Z_OK) {
	ctunnel_log(stderr, LOG_CRIT, "Error Initializing Zlib (inflate)");
	exit(1);
    }

    return comp;
}
void z_compress_reset(struct Compress comp)
{
    deflateReset(&comp.deflate);
    inflateReset(&comp.inflate);
}
void z_compress_end(struct Compress comp)
{
    deflateEnd(&comp.deflate);
    inflateEnd(&comp.inflate);
}
struct Packet z_compress(z_stream str, unsigned char *data, int size)
{
    //int ret = 0;
    struct Packet comp;

    comp.data = calloc(1, sizeof(char *) * 20048);


    str.avail_in = size;
    str.next_in = data;
    do {
        str.avail_out = 20048;
        str.next_out = comp.data;
        //ret = deflate(&str, Z_PARTIAL_FLUSH);
        deflate(&str, Z_PARTIAL_FLUSH);
        comp.len = 20048 - str.avail_out;
    } while (str.avail_out == 0);

    return comp;
}

struct Packet z_uncompress(z_stream str, unsigned char *data, int size)
{
//    int ret = 0;
    struct Packet comp;

    comp.data = calloc(1, sizeof(char *) * 20048);

    str.avail_in = size;
    str.next_in = data;
    str.avail_out = 20048;
    str.next_out = comp.data;

    //ret = inflate(&str, Z_FINISH);
    inflate(&str, Z_FINISH);
    comp.len = 20048 - str.avail_out;

    return comp;
}
