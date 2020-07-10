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
char *net_resolv(char *ip)
{
    struct hostent *ht;
    char *host = malloc(sizeof(char *) * 255);

    if (!isdigit(ip[0]))
    {
        if ((ht = gethostbyname(ip)))
        {
            memset(host, 0x00, sizeof(char *) * 255);
            snprintf(host, 32, "%s",
                     inet_ntoa(*(struct in_addr *)ht->h_addr_list[0]));
            return host;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        return NULL;
    }
    return NULL;
}
struct Network *udp_bind(char *ip, int port, int timeout)
{
#ifdef _WIN32
    WSADATA wsadata;
    int error;
    error = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if (error != NO_ERROR)
    {
        ctunnel_log(stderr, LOG_CRIT, "Error WSAStartup()\n");
        exit(1);
    }
#endif
    struct Network *udp = calloc(1, sizeof(struct Network));
    struct timeval tv;

    udp->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    udp->addr.sin_family = AF_INET;
    if (!strcmp("*", ip))
        udp->addr.sin_addr.s_addr = INADDR_ANY;
    else
        udp->addr.sin_addr.s_addr = inet_addr(ip);
    udp->addr.sin_port = htons(port);
    udp->len = sizeof(netsockaddr_in);

    if (bind(udp->sockfd, (netsockaddr *)&udp->addr, udp->len) == -1)
    {
        ctunnel_log(stderr, LOG_CRIT, "UDP Bind %s (port %d)", strerror(errno), port);
        exit(1);
    }
    if (timeout > 0)
    {
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(udp->sockfd, SOL_SOCKET, SO_SNDTIMEO, (sockopt_timeval *)&tv, sizeof(tv));
        setsockopt(udp->sockfd, SOL_SOCKET, SO_RCVTIMEO, (sockopt_timeval *)&tv, sizeof(tv));
    }

    return udp;
}

struct Network *udp_connect(char *ip, int port, int timeout)
{
#ifdef _WIN32
    WSADATA wsadata;
    int error;
    error = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if (error != NO_ERROR)
    {
        ctunnel_log(stderr, LOG_CRIT, "Error WSAStartup()\n");
        exit(1);
    }
#endif
    struct Network *udp = calloc(1, sizeof(struct Network));
    char *host;
    struct timeval tv;

    if ((udp->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        ctunnel_log(stderr, LOG_CRIT, "UDP Socket() %s", strerror(errno));
        exit(1);
    }
    udp->addr.sin_family = AF_INET;
    if (!(host = net_resolv(ip)))
        host = ip;
    udp->addr.sin_addr.s_addr = inet_addr(host);
    udp->addr.sin_port = htons(port);
    udp->len = sizeof(netsockaddr_in);

    ctunnel_log(stdout, LOG_INFO, "UDP Connect to %s:%d", host, port);

    free(host);

    if (timeout > 0)
    {
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(udp->sockfd, SOL_SOCKET, SO_SNDTIMEO, (sockopt_timeval *)&tv, sizeof(tv));
        setsockopt(udp->sockfd, SOL_SOCKET, SO_RCVTIMEO, (sockopt_timeval *)&tv, sizeof(tv));
    }

    return udp;
}

netsock tcp_listen(char *ip, int port)
{
#ifdef _WIN32
    WSADATA wsadata;
    int error;
    error = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (error != NO_ERROR)
    {
        ctunnel_log(stderr, LOG_CRIT, "Error WSAStartup()\n");
        exit(1);
    }
    SOCKADDR_IN sin;
#else
    struct sockaddr_in sin;
#endif
    netsock sockfd;
    char *host;
    int x = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&x, sizeof(x));
#else
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
#endif
    if (sockfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    sin.sin_family = AF_INET;
    if (!strcmp(ip, "*"))
    {
        sin.sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        if (!(host = net_resolv(ip)))
            host = ip;
        sin.sin_addr.s_addr = inet_addr(host);
    }
    sin.sin_port = htons(port);

    if (bind(sockfd, (netsockaddr *)&sin, sizeof(sin)) == -1)
    {
        perror("bind()");
        exit(1);
    }
    if (listen(sockfd, 8) == -1)
    {
        perror("listen()");
        exit(1);
    }
    return sockfd;
}
netsock tcp_accept(int sockfd)
{
#ifndef _WIN32
    struct sockaddr_in pin;
    socklen_t addrsize = 0;

    return accept(sockfd, (struct sockaddr *)&pin,
                  &addrsize);
#else
    return accept(sockfd, NULL, NULL);
#endif
}
netsock tcp_connect(char *ip, int port)
{
    netsockaddr_in address;
    netsock clifd;
    int servfd;
    struct hostent *ht;
    char host[255] = "";

    clifd = socket(AF_INET, SOCK_STREAM, 0);
    if (clifd < 0)
    {
        perror("socket");
        exit(1);
    }

    if (!isdigit(ip[0]))
    {
        ht = gethostbyname(ip);
        memset(host, 0x00, 255);
        snprintf(host, 32, "%s",
                 inet_ntoa(*(struct in_addr *)ht->h_addr_list[0]));
    }
    else
    {
        snprintf(host, 254, "%s", ip);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host);
    address.sin_port = htons(port);

    servfd = connect(clifd, (netsockaddr *)&address, sizeof(address));

    if (servfd < 0)
    {
        ctunnel_log(stderr, LOG_CRIT, "tcp_connect() / connect(): %s", strerror(errno));
        return servfd;
    }

    return clifd;
}
int net_read(struct Ctunnel *ct, struct Network *net, unsigned char *data, int len, int enc)
{
    int ret = 0;
    struct Packet pkt_out = {NULL, 0, 0, "", "", 0};
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

    if (ct->opt.proto == TCP)
    {
        ret = (int)recv(net->sockfd, (netbuf *)data, len, 0);
    }
    else
    {
        ret = recvfrom(net->sockfd, (netbuf *)data, len,
                       0, (netsockaddr *)&net->addr, &net->len);
    }

    if (ret < 0)
        return ret;
    if (enc == 1)
    {
        if (ct->opt.comp == 1)
        {
            if (ct->opt.proto == UDP)
                ct->comp = z_compress_init(ct->opt);
            pkt_out = z_uncompress(ct->comp.inflate, data, ret);
            if (ct->opt.proto == UDP)
                z_compress_end(ct->comp);
            memcpy(data, pkt_out.data, pkt_out.len);
            ret = pkt_out.len;
            free(pkt_out.data);
        }
        if (ct->opt.proxy == 0)
        {
            pkt_out = do_decrypt(ct->dctx, data, ret);
            memcpy(data, pkt_out.data, pkt_out.len);
            ret = pkt_out.len;
            free(pkt_out.data);
        }
    }
    //        fprintf(stdout, "R: [%s] -> [%s]\n",
    //                         inet_ntoa(((struct ip *)data)->ip_dst),
    //                         inet_ntoa(((struct ip *)data)->ip_src));

    //    fprintf(stdout, "R: [%s]%d\n", data, ret);
    net->rate.rx.total += ret;

    return ret;
}
int net_write(struct Ctunnel *ct, struct Network *net, unsigned char *data, int len, int enc)
{
    int ret = 0;
    struct Packet pkt_out = {NULL, 0, 0, "", "", 0};
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

    if (enc == 1)
    {
        if (ct->opt.proxy == 0)
        {
            pkt_out = do_encrypt(ct->ectx, data, len);
            memcpy(data, pkt_out.data, pkt_out.len);
            len = pkt_out.len;
            free(pkt_out.data);
        }
        if (ct->opt.comp == 1)
        {
            if (ct->opt.proto == UDP)
                ct->comp = z_compress_init(ct->opt);
            pkt_out = z_compress(ct->comp.deflate, data, len);
            if (ct->opt.proto == UDP)
                z_compress_end(ct->comp);
            memcpy(data, pkt_out.data, pkt_out.len);
            len = pkt_out.len;
            free(pkt_out.data);
        }
    }

    if (ct->opt.proto == TCP)
    {
        ret = (int)send(net->sockfd, (netbuf *)data, len, 0);
    }
    else
    {
        ret = sendto(net->sockfd, (netbuf *)data, len, 0,
                     (netsockaddr *)&net->addr,
                     (netsocklen_t)sizeof(netsockaddr));
    }

    net->rate.tx.total += ret;

    return ret;
}
void net_close(struct Network *net)
{
#ifdef _WIN32
    closesocket(net->sockfd);
#else
    close(net->sockfd);
#endif
}
