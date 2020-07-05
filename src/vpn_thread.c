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

#include "ctunnel.h"

#ifndef _WIN32
struct Network *get_addr(unsigned char *data)
{
	int i = 0, j = 0;
	unsigned long s, d, m;

	for (i = 2; i < MAX_CLIENTS; i++)
	{
		if (hosts[i].id > 0)
		{
			/*
	    fprintf(stdout, "s[%s/",
	            inet_ntoa(((struct ip *)data)->ip_dst));
	    fprintf(stdout, "%s",
	            inet_ntoa(((struct ip *)data)->ip_src));
	    fprintf(stdout, " d[%s]\n",
		    inet_ntoa(hosts[i].addr));
	    */
			for (j = 0; j < hosts[i].ni; j++)
			{
				s = ntohl(((struct ip *)data)->ip_dst.s_addr);
				d = hosts[i].n[j].dst;
				m = hosts[i].n[j].msk;
				//fprintf(stdout, "j[%d]%d\n", j, ((s&m) == (d&m)));
				if ((s & m) == (d & m))
				{
					fprintf(stdout, "-> n[%s] t[%s] %d\n", hosts[i].natip, hosts[i].tunip, hosts[i].net->sockfd);
					return hosts[i].net;
				}
			}
			if (((struct ip *)data)->ip_dst.s_addr == hosts[i].addr.s_addr)
			{
				fprintf(stdout, "   n[%s] t[%s]\n", hosts[i].natip, hosts[i].tunip);
				return hosts[i].net;
			}
		}
	}
	fprintf(stdout, "NULL\n");
	return NULL;
}
#endif
/*
void ping(struct Ctunnel *ct, struct Network *net)
{
    unsigned char *data = NULL;
    unsigned char *rwind = NULL;
    rwind = data = malloc(ct->opt.packet_size);
    //rwind = data;

    *data++ = PACKET_PING;
    memcpy(&data, &ct->id, sizeof(int));
//    gethostname((char *)data, 254);
    data = rwind;
    net_write(ct, net, data, ct->opt.packet_size, 1);
    free(data);
}
void ping_hosts(struct Ctunnel *ct)
{
    int i = 0;


    for (i = 2; i < MAX_CLIENTS; i++) {
	if (hosts[i].id > 0)
	    ping(ct, hosts[i].net);
//	    net_write(ct, hosts[i].net, data, sizeof(int) * 2, 1);
//	    ct->seen = time(NULL);
    }
}
*/
void del_host(int i)
{
	ctunnel_log(stdout, LOG_INFO, "Removing client %s[%s]",
				hosts[i].hostname, hosts[i].natip);
	memset(hosts[i].natip, 0x00, sizeof(hosts[i].natip));
	memset(hosts[i].tunip, 0x00, sizeof(hosts[i].tunip));
	memset(hosts[i].hostname, 0x00, sizeof(hosts[i].hostname));
	hosts[i].time = (time_t)0;
	hosts[i].id = 0;
}
int connected(struct Network *n)
{
	int i = 0;
	for (i = 2; i < MAX_CLIENTS; i++)
	{
		if (!strcmp(hosts[i].natip,
					inet_ntoa((struct in_addr)n->addr.sin_addr)))
		{
			return 0;
		}
	}
	return -1;
}
int add_host(struct Network *n, char *host, char *pool)
{
	int i = 0;

	for (i = 2; i < MAX_CLIENTS; i++)
	{
		if (!strcmp(hosts[i].hostname, host))
		{
			memcpy(hosts[i].net, n, sizeof(struct Network));
			strcpy(hosts[i].natip,
				   inet_ntoa((struct in_addr)n->addr.sin_addr));
			strcpy(hosts[i].hostname, host);
			snprintf(hosts[i].tunip, 32, "%s.%d", pool, i);
			ctunnel_log(stdout, LOG_INFO,
						"Host %s[%s] -> %s reusing ID %d",
						hosts[i].hostname,
						hosts[i].natip,
						hosts[i].tunip, i);
			hosts[i].addr.s_addr = inet_addr(hosts[i].tunip);
			hosts[i].time = time(NULL);
			hosts[i].id = i;
			return i;
		}
	}
	for (i = 2; i < MAX_CLIENTS; i++)
	{
		if (strlen(hosts[i].natip) == 0)
		{
			hosts[i].net = calloc(1, sizeof(struct Network));
			memcpy(hosts[i].net, n, sizeof(struct Network));
			strcpy(hosts[i].natip,
				   inet_ntoa((struct in_addr)n->addr.sin_addr));
			strcpy(hosts[i].hostname, host);
			snprintf(hosts[i].tunip, 32, "%s.%d", pool, i);
			hosts[i].addr.s_addr = inet_addr(hosts[i].tunip);
			ctunnel_log(stdout, LOG_INFO,
						"Host %s[%s] -> %s now has ID %d",
						hosts[i].hostname,
						hosts[i].natip,
						hosts[i].tunip, i);
			hosts[i].time = time(NULL);
			hosts[i].id = i;
			return i;
		}
	}
	return 0;
}

void vpn_thread(void *arg)
{
	struct Ctunnel *ct;
	struct Network *n;
	int ret = 0;
	time_t t, tl;
	unsigned char *data = NULL;
	unsigned char *rwind = NULL;
	struct timeval tv;
	fd_set rfd;
#ifdef HAVE_OPENSSL
	do_encrypt = openssl_do_encrypt;
	do_decrypt = openssl_do_decrypt;
	crypto_init = openssl_crypto_init;
	crypto_deinit = openssl_crypto_deinit;
	crypto_reset = openssl_crypto_reset;
#else
	do_encrypt = gcrypt_do_encrypt;
	do_decrypt = gcrypt_do_decrypt;
	crypto_init = gcrypt_crypto_init;
	crypto_deinit = gcrypt_crypto_deinit;
	crypto_reset = gcrypt_crypto_reset;
#endif
	struct xfer_stats rx_st, tx_st;

	t = tl = time(NULL);
	ct = (struct Ctunnel *)arg;

	if (ct->opt.comp == 1)
		ct->comp = z_compress_init(ct->opt);

	/* Data size, plus int for channel */
	data = malloc(ct->opt.packet_size);
	rwind = data;

	xfer_stats_init(&tx_st, (int)t);
	xfer_stats_init(&rx_st, (int)t);
	if (ct->opt.stats == 1)
		xfer_stats_print(stdout, &tx_st, &rx_st);

	while (1)
	{
		FD_ZERO(&rfd);
		FD_SET(ct->net_srv->sockfd, &rfd);
		FD_SET(ct->tunfd, &rfd);
		//FD_SET(ct->p[0], &rfd);

		tv.tv_sec = 10;
		tv.tv_usec = 0;

		if (ct->opt.proto == UDP)
		{
			crypto_reset(ct->ectx, ct->opt, 1);
			crypto_reset(ct->dctx, ct->opt, 0);
		}
		ret = select(ct->net_srv->sockfd + ct->tunfd, &rfd,
					 NULL, NULL, &tv);
		// for ppp
		//ret = select(ct->net_srv->sockfd + ct->tunfd, &rfd,
		//	     NULL, NULL, NULL);
		t = time(NULL);

		if (ct->opt.stats == 1)
			xfer_stats_print(stdout, &tx_st, &rx_st);

		/* Not functioning 
	if (ret == 0 || (t > tl+10)) { 
	    if (ct->opt.role != SERVER) {
	    	ping(ct, ct->net_srv);
	    }
	    tl = t;
	}
	Need to redial server if connection timeout, make sure to
	re-resolv address before connect
	*/

		if (ret > 0)
		{
			n = ct->net_srv;
			if (FD_ISSET(ct->tunfd, &rfd))
			{
				*data++ = PACKET_DATA;
				ret = read(ct->tunfd, data, ct->opt.packet_size);
				data = rwind;
				if (ret > 0 && n)
				{
					if ((ret = net_write(ct, n, data,
										 ret + sizeof(int), 1)) < 0)
						ctunnel_log(stderr, LOG_CRIT,
									"net_write:%d %s", __LINE__,
									strerror(errno));
					xfer_stats_update(&tx_st, ct->net_srv->rate.tx.total, t);
				}
				else
				{
					ctunnel_log(stderr, LOG_CRIT,
								"tunfd read:%d %s", __LINE__,
								strerror(errno));
				}
			}
			if (FD_ISSET(ct->net_srv->sockfd, &rfd))
			{
				ret = net_read(ct, n, data,
							   ct->opt.packet_size, 1);
				if (ret > 0)
				{
					xfer_stats_update(&rx_st, ct->net_srv->rate.rx.total, t);
					if (*data == PACKET_DATA)
					{
						if ((connected(n) < 0) && (ct->opt.role == SERVER))
						{
							data = rwind;
							*data++ = PACKET_REUP;
							data = rwind;
							net_write(ct, n, data, sizeof(int) * 2, 1);
						}
						else
						{
							data++;
							if (write(ct->tunfd, data, ret - sizeof(int)) == -1)
							{
								ctunnel_log(stderr, LOG_CRIT, "write() error");
							}
							data = rwind;
						}
					}
					if (*data == PACKET_CONRQ)
					{
						ctunnel_log(stdout, LOG_INFO, "Connect request from %s",
									inet_ntoa((struct in_addr)
												  ct->net_srv->addr.sin_addr));
						vpn_handshake(ct);
					}
					/* Not functioning
		   if (*data == PACKET_PING) {
		       ct->seen = time(NULL);
		       if (ct->opt.role == SERVER) 
			   ping(ct, ct->net_srv);
		   }
		   */
					if ((*data == PACKET_REUP) && ct->opt.role != SERVER)
					{
						vpn_handshake(ct);
					}
				}
				else
				{
					ctunnel_log(stderr, LOG_CRIT, "net_read:74 %s",
								strerror(errno));
					break;
				}
			}
		}
	}
	FD_CLR(ct->net_srv->sockfd, &rfd);
	FD_CLR(ct->tunfd, &rfd);
	pthread_mutex_lock(&mutex);
	threads[ct->id] = 2;
	pthread_mutex_unlock(&mutex);
	net_close(ct->net_srv);
	free(data);
	fprintf(stdout, "VPN Disconnect\n");
	crypto_deinit(ct->ectx);
	crypto_deinit(ct->dctx);
	pthread_exit(0);
}
