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
#include <pthread.h>

#ifndef _WIN32
#include <net/route.h>
#endif

#include "ctunnel.h"

void vpn_loop(struct options opt)
{
	struct Ctunnel **ctunnel;
	struct Network *net_srv = NULL;
	char dev[5] = "";
	int i = 0, x = 0, srv_sockfd = 0, ret = 0, tunfd = 0;
	int pool = 1;
	pthread_t tid[MAX_THREADS];
	extern int threads[MAX_THREADS];
#ifdef HAVE_OPENSSL
	crypto_init = openssl_crypto_init;
	crypto_deinit = openssl_crypto_deinit;
#else
	crypto_init = gcrypt_crypto_init;
	crypto_deinit = gcrypt_crypto_deinit;
#endif

	pthread_mutex_init(&mutex, NULL);

	for (i = 0; i < MAX_THREADS; i++)
		threads[i] = 0;
#ifndef _WIN32
	for (i = 0; i < MAX_THREADS; i++)
		tid[i] = (pthread_t)0;
#endif
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		memset(hosts[i].natip, 0x00, sizeof(hosts[i].natip));
		memset(hosts[i].tunip, 0x00, sizeof(hosts[i].tunip));
		//memset(hosts[i].tunip, 0x00, sizeof(hosts[i].hostname));
		hosts[i].time = (time_t)0;
		hosts[i].id = 0;
	}
	i = 0;

	ctunnel = malloc(sizeof(struct Ctunnel **) * MAX_THREADS);
	if (!ctunnel)
	{
		fprintf(stderr, "MALLOC %s\n", strerror(errno));
		exit(1);
	}

	if (opt.role == SERVER)
	{
		sprintf(opt.gw, "%s.%d", opt.pool, pool);
		if (opt.vmode == VMODE_TUN)
		{
			sprintf(dev, "tun%d", opt.tun);
			if ((tunfd = mktun(dev)) < 0)
			{
				fprintf(stderr, "mktun: %s\n", strerror(errno));
				exit(1);
			}
			ifconfig(dev, opt.gw, opt.gw, "255.255.255.0");
		}
		else
		{
			sprintf(dev, "ppp%d", opt.tun);
		}
		ctunnel_log(stdout, LOG_INFO, "PtP Pool Local Address %s",
					opt.gw);
	}

	if (opt.proto == TCP)
	{
		if (opt.role == SERVER)
			srv_sockfd = tcp_listen(opt.local.ip, opt.local.ps);
		net_srv = calloc(1, sizeof(struct Network));
	}
	if (srv_sockfd < 0)
	{
		ctunnel_log(stderr, LOG_CRIT, "listen() %s\n", strerror(errno));
		exit(1);
	}

	/*
    for (i = 0; i < MAX_CLIENTS; i++) {
	memset(hosts[i].natip, 0x00, 32);
	memset(hosts[i].tunip, 0x00, 32);
    }
*/
	i = 0;
	if (opt.role == SERVER)
	{
		while (1)
		{
			if (opt.proto == TCP)
				net_srv->sockfd = tcp_accept(srv_sockfd);
			else
				net_srv = udp_bind(opt.local.ip, opt.local.ps, 0);

			i = clients = 0;

			if (net_srv->sockfd > 0)
			{

				pthread_mutex_lock(&mutex);
				for (i = 0; i != MAX_THREADS; i++)
				{
					if (threads[i] == 0)
						break;
					if (i == MAX_THREADS)
						ctunnel_log(stderr, LOG_CRIT,
									"Max Threads %d "
									"reached!",
									i);
				}
				ctunnel[i] = malloc(sizeof(struct Ctunnel));
				ctunnel[i]->net_srv = net_srv;
				ctunnel[i]->srvsockfd = net_srv->sockfd;
				ctunnel[i]->tunfd = tunfd;
				ctunnel[i]->opt = opt;
				ctunnel[i]->id = i;
				ctunnel[i]->srv_sockfd = srv_sockfd;
				ctunnel[i]->ectx = NULL;
				ctunnel[i]->dctx = NULL;
				ctunnel[i]->ectx = crypto_init(opt, 1);
				ctunnel[i]->dctx = crypto_init(opt, 0);
				pthread_mutex_unlock(&mutex);

				threads[i] = 2;
				if (ctunnel[i]->opt.vmode == VMODE_PPP)
					ctunnel[i]->tunfd = prun(ctunnel[i]->opt.ppp);
				if (opt.proto == UDP)
				{
					ctunnel_log(stdout, LOG_INFO, "UDP Waiting");
					vpn_thread(ctunnel[i]);
				}
				else
				{
					ret = pthread_create(&tid[i], NULL,
										 (void *)vpn_thread,
										 (void *)ctunnel[i]);
					if (ret != 0)
						fprintf(stderr, "Cannot create thread\n");
					for (x = 0; x != MAX_THREADS; x++)
					{
						if (threads[x] == 2)
						{
							fprintf(stdout, "Joining thread %d\n", x);
							pthread_join(tid[x], NULL);
							pthread_mutex_lock(&mutex);
							threads[x] = 0;
							pthread_mutex_unlock(&mutex);
							free(ctunnel[x]);
						}
					}
				}
			}
			else
			{
				net_close(net_srv);
			}
		}
	}
	else
	{
		ctunnel[i] = malloc(sizeof(struct Ctunnel));
		ctunnel[i]->opt = opt;
		ctunnel[i]->srv_sockfd = srv_sockfd;
		ctunnel[i]->id = i;
		ctunnel[i]->ectx = crypto_init(opt, 1);
		ctunnel[i]->dctx = crypto_init(opt, 0);
		ctunnel[i]->nets_t = 0;
		if (opt.proto == TCP)
		{
			net_srv->sockfd = tcp_connect(opt.remote.ip, opt.remote.ps);
			ctunnel[i]->net_srv = net_srv;
		}
		else
		{
			ctunnel[i]->net_srv =
				udp_connect(opt.remote.ip, opt.remote.ps, 5);
		}
		if (ctunnel[i]->opt.vmode == VMODE_PPP)
			ctunnel[i]->tunfd = prun(ctunnel[i]->opt.ppp);
		sprintf(dev, "tun%d", ctunnel[i]->opt.tun);
		if (opt.vmode == VMODE_TUN)
		{
			if ((ctunnel[i]->tunfd = mktun(dev)) < 0)
			{
				ctunnel_log(stderr, LOG_CRIT,
							"Unable to create TUN device, aborting!");
				exit(1);
			}
		}
		if ((vpn_handshake(ctunnel[i])) < 0)
		{
			ctunnel_log(stderr, LOG_CRIT, "Handshake Error");
			crypto_deinit(ctunnel[i]->ectx);
			crypto_deinit(ctunnel[i]->dctx);
			free(ctunnel[i]);
			free(ctunnel);
			exit(1);
		}
		vpn_thread(ctunnel[i]);
	}
}
