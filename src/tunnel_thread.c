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

#include <pthread.h>

#include "ctunnel.h"

void *ctunnel_mainloop(void *arg)
{
    struct Ctunnel *ct;
    struct Network *net_srv;
    struct Network *net_cli;
    unsigned char *data;
    fd_set rfd;
    int ret = 0, dir = 0;
    extern int threads[MAX_THREADS];
    struct timeval tv;
#ifdef HAVE_OPENSSL
    do_encrypt = openssl_do_encrypt;
    do_decrypt = openssl_do_decrypt;
    crypto_init = openssl_crypto_init;
    crypto_deinit = openssl_crypto_deinit;
#else
    do_encrypt = gcrypt_do_encrypt;
    do_decrypt = gcrypt_do_decrypt;
    crypto_init = gcrypt_crypto_init;
    crypto_deinit = gcrypt_crypto_deinit;
#endif

    ct = (struct Ctunnel *) arg;

    if (ct->opt.proto == TCP) {
	net_srv = calloc(1, sizeof(struct Network));
	net_cli = calloc(1, sizeof(struct Network));
	net_cli->sockfd = (int) ct->clisockfd;
	net_srv->sockfd = (int) ct->srvsockfd;
    } else {
	net_srv = (struct Network *) ct->net_srv;
	net_cli = (struct Network *) ct->net_cli;
    }

    if (ct->opt.proxy == 0) {
	ct->ectx = crypto_init(ct->opt, 1);
	ct->dctx = crypto_init(ct->opt, 0);
    }


    if (ct->opt.comp == 1)
	ct->comp = z_compress_init(ct->opt);

    data = calloc(1, sizeof(char *) * ct->opt.packet_size);

    while (1) {
	if (ct->opt.proto == UDP && ct->opt.proxy == 0) {
	    crypto_deinit(ct->ectx);
	    crypto_deinit(ct->dctx);
	    ct->ectx = crypto_init(ct->opt, 1);
	    ct->dctx = crypto_init(ct->opt, 0);
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;

	FD_ZERO(&rfd);
	FD_SET(net_srv->sockfd, &rfd);
	FD_SET(net_cli->sockfd, &rfd);
	ret = select(net_srv->sockfd + net_cli->sockfd + 1, &rfd,
		     NULL, NULL, &tv);
	if (ret < 0)
	    break;

	if (FD_ISSET(net_srv->sockfd, &rfd)) {
	    if (ct->opt.role == 0)
		dir = 0;
	    if (ct->opt.role == 1)
		dir = 1;

	    ret = net_read(ct, net_srv, data, ct->opt.packet_size, dir);

	    if (ct->opt.role == 0)
		dir = 1;
	    if (ct->opt.role == 1)
		dir = 0;

	    ret = net_write(ct, net_cli, data, ret, dir);

	    memset(data, 0x00, sizeof(char *) * ct->opt.packet_size);

	    if (ret <= 0) /* Connection finished */ 
		break;
	}
	if (FD_ISSET(net_cli->sockfd, &rfd)) {
	    if (ct->opt.role == 0)
		dir = 1;
	    if (ct->opt.role == 1)
		dir = 0;

	    ret = net_read(ct, net_cli, data, ct->opt.packet_size, dir);
	    if (ret <= 0)
		break;

	    if (ct->opt.role == 0)
		dir = 0;
	    if (ct->opt.role == 1)
		dir = 1;

	    ret = net_write(ct, net_srv, data, ret, dir);

	    memset(data, 0x00, sizeof(char *) * ct->opt.packet_size);

	    if (ret <= 0) {
		fprintf(stdout, "net_write %s:%d\n", __FILE__, __LINE__);
		break;
	    }
	}
	if (ct->opt.proto == UDP && ct->opt.comp == 1) {
	    z_compress_reset(ct->comp);
	}
	memset(data, 0x00, sizeof(char *) * ct->opt.packet_size);
    }
    /* Connection closed */
    FD_CLR(net_cli->sockfd, &rfd);
    FD_CLR(net_srv->sockfd, &rfd);

    if (ct->opt.proto == TCP) {
	pthread_mutex_lock(&mutex);
	threads[ct->id] = 2;
	pthread_mutex_unlock(&mutex);
	//fprintf(stdout, "THREAD: Exit %d\n", ct->id);
	net_close(net_cli);
	net_close(net_srv);
	if (ct->opt.proxy == 0) {
	    crypto_deinit(ct->ectx);
	    crypto_deinit(ct->dctx);
	}
	if (ct->opt.comp == 1) {
	    z_compress_end(ct->comp);
	}

	free(net_cli);
	free(net_srv);
    }
    //free(ct);
    free(data);
    if (ct->opt.proto == TCP)
	pthread_exit(0);
    return NULL;
}
