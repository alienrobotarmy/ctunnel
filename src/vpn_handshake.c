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

#ifndef _WIN32
#include <net/route.h>
#endif

#include "ctunnel.h"

int vpn_handshake(void *arg)
{
    struct Ctunnel *ct;
    char hostname[255] = "";
    unsigned char *data = NULL;
    unsigned char *rwind = NULL;
    char dev[5] = "";
    int ret = 0, clientaddr = 2;
#ifdef HAVE_OPENSSL
    crypto_init = openssl_crypto_init;
    crypto_deinit = openssl_crypto_deinit;
    do_decrypt = openssl_do_decrypt;
    do_encrypt = openssl_do_encrypt;
#else
    crypto_init = gcrypt_crypto_init;
    crypto_deinit = gcrypt_crypto_deinit;
    do_decrypt = gcrypt_do_decrypt;
    do_encrypt = gcrypt_do_encrypt;
#endif

    ct = (struct Ctunnel *) arg;
    data = malloc((ct->opt.packet_size));
    memset(data, 0x00, ct->opt.packet_size);
    rwind = data;

    if (ct->opt.role == SERVER) {
	ret = net_read(ct, ct->net_srv, data, ct->opt.packet_size, 1);
	if (*data == PACKET_HOSTN) {
	    data++;
	    data[ret-sizeof(int)] = '\0';
	    clientaddr = add_host(ct->net_srv, (char *) data, ct->opt.pool);
	} else {
	    fprintf(stderr, "[ctunnel] Error: Packets out of order\n");
	    return 0;
	}

	data = rwind;
	*data++ = PACKET_CLNTR;
	*data = clientaddr;
	data = rwind;
	ret = net_write(ct, hosts[clientaddr].net, data,
		        sizeof(int) * 4, 1);

	if (ct->opt.networks) {
	    *data++ = PACKET_ROUTE;
	    memcpy(data, ct->opt.networks, strlen(ct->opt.networks)+1);
	    data = rwind;
	    ret = net_write(ct, hosts[clientaddr].net, data,
		            sizeof(int) + strlen(ct->opt.networks), 1);
	} else {
	    *data++ = PACKET_NRTE;
	    data = rwind;
	    ret = net_write(ct, hosts[clientaddr].net, data, sizeof(int) *2, 1);
	}

	/* Get remote routes */
	memset(data, 0x00, ct->opt.packet_size);
	ret = net_read(ct, hosts[clientaddr].net, data, ct->opt.packet_size, 1);
	if (ct->opt.vmode == VMODE_TUN)
	    sprintf(dev, "tun%d", ct->opt.tun);
	else
	    sprintf(dev, "ppp%d", ct->opt.tun);
	if (*data == PACKET_ROUTE) {
	    data++;
	    if (data) {
	        ctunnel_log(stdout, LOG_INFO, "Networks [%s]", data);
	        if ((add_routes(ct, ct->opt.gw, dev, clientaddr, (char *) data)) != 0)
		    ctunnel_log(stdout, LOG_INFO, "Unable to add route", data);
	    }
	} else {
	    data++;
	    ctunnel_log(stdout, LOG_INFO, "No routes from peer");
	}
	data = rwind;

	clients++;
    } else {
	*data++ = PACKET_CONRQ; *data = '\0'; data = rwind;
	ret = net_write(ct, ct->net_srv, data, sizeof(int) * 2, 1);
	gethostname(hostname, 254);
	ctunnel_log(stdout, LOG_INFO,
	            "Sending hostname %s", hostname);

	*data++ = PACKET_HOSTN;
	memcpy(data, hostname, strlen(hostname)+1);
	data = rwind;
	ret = net_write(ct, ct->net_srv, data,
		        sizeof(int) + strlen(hostname), 1);

        memset(data, 0x00, ct->opt.packet_size);
	ret = net_read(ct, ct->net_srv, data, ct->opt.packet_size, 1);
	if (ret == 0) {
	    free(rwind);
	    return -1;
	}
	data++;
	ct->id = *data;
	if (ct->id == 0) {
	    ctunnel_log(stdout, LOG_INFO, "Problem with remote endpoint");
	    return -1;
	}

	sprintf(ct->src, "%s.%d", ct->opt.pool, ct->id);
        sprintf(ct->opt.gw, "%s.1", ct->opt.pool);
	if (ct->opt.vmode == VMODE_PPP) {
	    sprintf(dev, "ppp%d", ct->opt.tun);
//            ct->tunfd = prun(ct->opt.ppp); 
	} else {
//	    sprintf(dev, "tun%d", ct->opt.tun);
	    /* create tun on startup to allow for dynamic renegotiation
	    if ((ct->tunfd = mktun(dev)) < 0) {
                ctunnel_log(stderr, LOG_CRIT, 
		"Unable to create TUN device, aborting!");
                return -1;
            }
	    */
	    sprintf(dev, "tun%d", ct->opt.tun);
	    ifconfig(dev, ct->opt.gw, ct->src, "255.255.255.0");
	}
//	}
#ifndef _WIN32
        setenv("CTUNNEL_GATEWAY", ct->opt.gw, 1);
        setenv("CTUNNEL_SOURCE", ct->src, 1);
        setenv("CTUNNEL_DEV", dev, 1);
#endif
	ctunnel_log(stdout, LOG_INFO, "PtP ip:%s gw:%s",
		    ct->src, ct->opt.gw);

	data = rwind;

	/* Get remote routes */
        memset(data, 0x00, ct->opt.packet_size);
	ret = net_read(ct, ct->net_srv, data, ct->opt.packet_size, 1);
	if (ret == 0) {
	    free(rwind);
	    return -1;
	}

	if (*data == PACKET_ROUTE && ct->opt.vmode == VMODE_TUN) {
	    data++;
	    if (data) {
#ifndef _WIN32 
                setenv("CTUNNEL_ROUTES", (const char *) data, 1);
#endif
	        if ((add_routes(ct, ct->src, dev, 0, (char *) data)) != 0) {
		    free(rwind);
		    fprintf(stderr, "Problem adding route\n");
		    return -1;
	        }
	    }
	} else {
	    ctunnel_log(stdout, LOG_INFO, "No routes from peer");
	}

        data = rwind;
        if (ct->opt.networks) {
	    *data++ = PACKET_ROUTE;
	    memcpy(data, ct->opt.networks, strlen(ct->opt.networks)+1);
	    data = rwind;
	    ret = net_write(ct, ct->net_srv, data,
		            sizeof(int) + strlen(ct->opt.networks) + 1, 1);
	} else {
	    *data++ = PACKET_NRTE;
	    data = rwind;
	    ret = net_write(ct, ct->net_srv, data, sizeof(int) * 2, 1);
	}

	ct->seen = time(NULL);
    }
    free(rwind);
    if (ct->opt.pexec) {
        ctunnel_log(stderr, LOG_INFO, "Post UP EXEC: %s", ct->opt.pexec);
        if ((ret = run(ct->opt.pexec)) != 0) {
            ctunnel_log(stderr, LOG_CRIT, "Exec returned %d: %s", ret, strerror(errno));
        }
    }
    ctunnel_log(stdout, LOG_INFO, "VPN UP");
    return 0;
}
