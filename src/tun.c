/*
 * Ctunnel - Cyptographic tunnel.
 * Copyright (C) 2008-2014 Jess Mahan <ctunnel@nardcore.org>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/route.h>
#else
//#include <tap-windows.h>
#endif
#include <sys/types.h>
#ifdef __APPLE__
#include <net/if.h> // OSX ifreq
#include <net/if_dl.h> // OSX ifreq
#endif

#include <fcntl.h>

#ifdef __linux__
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

#include "ctunnel.h"

int add_routes(struct Ctunnel *ct, char *gw, char *dev, int id, char *networks)
{
    int ret = 0, i = 0;
    struct JToken *tok, *tok2;
    char dst[256] = "";
    char nm[256] = "";

    tok  = malloc(sizeof(struct JToken));
    tok2 = malloc(sizeof(struct JToken));
    tok->so = 0; tok->eo = 0; tok->match = NULL;

    ct->nets_t = 0;
    hosts[id].ni=0;

    while ((jtok(tok, networks, ','))) {
        tok2->so = 0; tok2->eo = 0; tok2->match = NULL;
	if ((jtok(tok2, tok->match, '/')) < 1) {
	    ctunnel_log(stderr, LOG_CRIT, "Specify CIDR Netmask with /");
	    ret++;
	} else {
	    strcpy(dst, tok2->match);
	    if ((jtok(tok2, tok->match, '/'))) {
		if ((route_add(dev, gw, dst, tok2->match)) < 0) {
		    ret++;
		} else {
		    i++;
		}
		cidr_to_mask(nm, atoi(tok2->match));
		hosts[id].n[i].dst = ntohl(inet_addr(dst));
		hosts[id].n[i].msk = ntohl(inet_addr(nm));
		hosts[id].ni++;
	    } else {
		ctunnel_log(stderr, LOG_CRIT, "Must Specify Netmask for %s", tok->match);
		ret++;
	    }
	}
	memset(dst, 0, 256);
	free(tok2->match);
    }
    free(tok->match);
    free(tok); free(tok2);
    return ret;
}
int route_add(char *dev, char *gw, char *dst, char *msk)
{
    int sockfd;
    char nm[256] = "";

    if ((cidr_to_mask(nm, atoi(msk))) < 0) {
	ctunnel_log(stderr, LOG_CRIT, "Invalid CIDR Netmask");
	return -1;
    }
#ifdef __linux__
    gw = NULL;
    struct rtentry rt;
    struct sockaddr_in irt_dst;
    struct sockaddr_in irt_msk;
    memset(&rt, 0, sizeof(struct rtentry));
    memset(&irt_dst, 0, sizeof(struct sockaddr));
    memset(&irt_msk, 0, sizeof(struct sockaddr));

    irt_dst.sin_family = AF_INET;
    irt_msk.sin_family = AF_INET;

    irt_dst.sin_addr.s_addr = inet_addr(dst);
    irt_msk.sin_addr.s_addr = inet_addr(nm);

    memcpy(&rt.rt_dst, &irt_dst, sizeof(struct sockaddr));
    memcpy(&rt.rt_genmask, &irt_msk, sizeof(struct sockaddr));

    rt.rt_flags = RTF_UP;
    rt.rt_dev = dev;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ((ioctl(sockfd, SIOCADDRT, &rt)) < 0) {
	close(sockfd);
	ctunnel_log(stderr, LOG_CRIT, "%s %s -> %s/%s", strerror(errno), dev, dst, msk);
	return -1;
    } else {
	ctunnel_log(stdout, LOG_INFO, "Route [%s] -> %s %s", dev, dst, nm);
    }
#elif !(defined _WIN32)
    struct {
        struct rt_msghdr msghdr;
        struct sockaddr_in addr[3];
	struct sockaddr_dl sdl;
    } msg;
    int seq, i = 0;

    memset(&msg, 0, sizeof(msg));
    msg.msghdr.rtm_msglen  = sizeof(msg);
    msg.msghdr.rtm_version = RTM_VERSION;
    msg.msghdr.rtm_type    = RTM_ADD;
    msg.msghdr.rtm_index   = if_nametoindex(dev);
    msg.msghdr.rtm_pid     = 0;
    msg.msghdr.rtm_addrs   = RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFP;
    msg.msghdr.rtm_seq     = ++seq;
    msg.msghdr.rtm_errno   = 0;
    msg.msghdr.rtm_flags   = RTF_UP | RTA_GATEWAY;
    for (i = 0; i < (int) (sizeof(msg.addr) / sizeof(msg.addr[0])); i++) {
        msg.addr[i].sin_len    = sizeof(msg.addr[0]);
        msg.addr[i].sin_family = AF_INET;
    }
    msg.addr[0].sin_addr.s_addr = inet_addr(dst);
    msg.addr[1].sin_addr.s_addr = inet_addr(gw);
    msg.addr[2].sin_addr.s_addr = inet_addr(nm);
    msg.sdl.sdl_family = AF_LINK;
    msg.sdl.sdl_len = sizeof(struct sockaddr_dl);
    link_addr(dev, &msg.sdl);
    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {
        close(sockfd);
        ctunnel_log(stderr, LOG_CRIT, "%s %s/%s", strerror(errno), dst, msk);
        return -1;
    } else {
        if ((i = write(sockfd, &msg, sizeof(msg))) > 0)
            ctunnel_log(stdout, LOG_INFO, "Route [%s/%d] -> %s %s %s",
	                dev, if_nametoindex(dev), dst, gw, nm);
        else
            ctunnel_log(stderr, LOG_CRIT, "add_route(%s -> %s/%s): %s", strerror(errno), dev, dst, nm);
    }
#elif defined _WIN32
       return 0;
#endif
    close(sockfd);
    return 0;
}

int mktun(char *dev)
{
    int fd = 0;
#ifndef _WIN32
    struct ifreq ifr;
#ifdef __APPLE__
    char path[1024];

    snprintf(path, 1024, "/dev/%s", dev);
    if ((fd = open(path, O_RDWR)) < 0) {
#else
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
#endif
	ctunnel_log(stderr, LOG_CRIT, "Unable to open TUN device: %s",
		    strerror(errno));
	 /* Might need to continue and just set address if renegotiating */
	return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
#if !(defined __APPLE__) && !(defined _WIN32)
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if ((ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
	ctunnel_log(stderr, LOG_CRIT, "ioctl(): %s", strerror(errno));
	return -1;
    }
#endif
#endif
    return fd;
}
int ifconfig(char *dev, char *gw, char *ip, char *netmask)
{
#ifndef _WIN32
    struct ifreq ifr;
    int sockfd;
    struct sockaddr_in addr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    memset(&addr, 0, sizeof(struct sockaddr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ((ioctl(sockfd, SIOCSIFADDR, (void *) &ifr)) < 0) {
	ctunnel_log(stderr, LOG_CRIT, "Cannot assign addres %s to %s: %s",
		    ip, dev, strerror(errno));
	return -1;
    }
    addr.sin_addr.s_addr = inet_addr(gw);
    memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ((ioctl(sockfd, SIOCSIFDSTADDR, (void *) &ifr)) < 0) {
        ctunnel_log(stderr, LOG_CRIT, "Cannot assign addres %s to %s: %s",
                    gw, dev, strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(struct sockaddr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(netmask);
    memcpy(&ifr.ifr_addr, &addr, sizeof(struct sockaddr));
    if ((ioctl(sockfd, SIOCSIFNETMASK, (void *) &ifr)) < 0) {
	ctunnel_log(stderr, LOG_CRIT, "Cannot assign netmask %s to %s: %s",
		    ip, dev, strerror(errno));
	return -1;
    }

    ifr.ifr_flags = IFF_UP | IFF_RUNNING;
    if ((ioctl(sockfd, SIOCSIFFLAGS, (void *) &ifr)) < 0) {
	ctunnel_log(stderr, LOG_CRIT, "Cannot UP interface %s: %s", dev,
		    strerror(errno));
	return -1;
    }
    close(sockfd);
#endif
    return 0;
}
