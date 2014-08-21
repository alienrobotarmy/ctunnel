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

void tunnel_loop(struct options *opt)
{
    struct Ctunnel **ctunnel;
    pthread_t tid[MAX_THREADS];
    extern int threads[MAX_THREADS];
    int i = 0, x = 0;
    struct timeval tv;
    int srv_sockfd = 0;
    struct Network *net_cli = NULL, *net_srv = NULL;
#ifndef _WIN32
    struct sockaddr_in pin;
    socklen_t addrsize = 0;
#endif

    pthread_mutex_init(&mutex, NULL);

    for (i = 0; i < MAX_THREADS; i++)
	threads[i] = 0;
#ifndef _WIN32
    for (i = 0; i < MAX_THREADS; i++)
	tid[i] = (pthread_t) 0;
#endif

    ctunnel = malloc(sizeof(struct Ctunnel **) * MAX_THREADS);

    i = 0;

    if (opt->proto == TCP) {
	srv_sockfd = tcp_listen(opt->local.ip, opt->local.ps);
	net_cli = calloc(1, sizeof(struct Network));
	net_srv = calloc(1, sizeof(struct Network));
    } else {
	net_srv = udp_bind(opt->local.ip, opt->local.ps, 0);
	net_cli = udp_connect(opt->remote.ip, opt->remote.ps, 0);
    }
    if (srv_sockfd < 0) {
	ctunnel_log(stderr, LOG_CRIT, "listen() %s\n", strerror(errno));
	free(ctunnel);
	exit(1);
    }

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    while (1) {
	setsockopt(srv_sockfd, SOL_SOCKET, SO_RCVTIMEO,
		   (sockopt_timeval *) &tv, sizeof(struct timeval));
	setsockopt(srv_sockfd, SOL_SOCKET, SO_SNDTIMEO,
		   (sockopt_timeval *) &tv, sizeof(struct timeval));
	setsockopt(net_cli->sockfd, SOL_SOCKET, SO_RCVTIMEO,
		   (sockopt_timeval *) &tv, sizeof(struct timeval));
	setsockopt(net_cli->sockfd, SOL_SOCKET, SO_SNDTIMEO,
		   (sockopt_timeval *) &tv, sizeof(struct timeval));
	setsockopt(net_srv->sockfd, SOL_SOCKET, SO_RCVTIMEO,
		   (sockopt_timeval *) &tv, sizeof(struct timeval));
	setsockopt(net_srv->sockfd, SOL_SOCKET, SO_SNDTIMEO,
		   (sockopt_timeval *) &tv, sizeof(struct timeval));
	if (opt->proto == TCP) {
#ifdef _WIN32
	    net_srv->sockfd = accept(srv_sockfd, NULL, NULL);
#else
	    net_srv->sockfd = accept(srv_sockfd, (struct sockaddr *) &pin,
				     &addrsize);
#endif
	    if (net_srv->sockfd > 0) {
		net_cli->sockfd = tcp_connect(opt->remote.ip, opt->remote.ps);

		if (net_cli->sockfd > 0) {

		    pthread_mutex_lock(&mutex);
		    for (i = 0; i != MAX_THREADS; i++) {
			if (threads[i] == 0)
			    break;
			if (i == MAX_THREADS)
			    ctunnel_log(stderr, LOG_CRIT,
					"Max Threads %d " "reached!", i);
		    }
		    ctunnel[i] = malloc(sizeof(struct Ctunnel));
		    ctunnel[i]->clisockfd = net_cli->sockfd;
		    ctunnel[i]->srvsockfd = net_srv->sockfd;
		    ctunnel[i]->opt = (*(opt));
		    ctunnel[i]->srv_sockfd = srv_sockfd;
		    ctunnel[i]->id = i;

		    pthread_mutex_unlock(&mutex);

		    threads[i] = 1;
		    pthread_create(&tid[i], NULL,
				   (void *) ctunnel_mainloop,
				   (void *) ctunnel[i]);
		    //pthread_detach(tid[i]);
		    for (x = 0; x != MAX_THREADS; x++) {
			if (threads[x] == 2) {
			    pthread_join(tid[x], NULL);
			    pthread_mutex_lock(&mutex);
			    threads[x] = 0;
			    free(ctunnel[x]);
			    pthread_mutex_unlock(&mutex);
			}
		    }
		}
	    } else {
		net_close(net_srv);
	    }
	} else {
	    while (1) {
		ctunnel[i] = malloc(sizeof(struct Ctunnel));
		ctunnel[i]->clisockfd = net_cli->sockfd;
		ctunnel[i]->srvsockfd = net_srv->sockfd;
		ctunnel[i]->net_cli = net_cli;
		ctunnel[i]->net_srv = net_srv;
		ctunnel[i]->opt = (*(opt));
		ctunnel[i]->srv_sockfd = srv_sockfd;
		ctunnel[i]->id = i;
		ctunnel_mainloop((void *) ctunnel[i]);
		free(ctunnel[i]);
	    }
	}
    }
}
