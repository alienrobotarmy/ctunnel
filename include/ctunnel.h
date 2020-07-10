#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/md5.h>
#else
#include <gcrypt.h>
#endif

#ifndef __WIN32
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_systm.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#else
#define LOG_CRIT 1
#define LOG_INFO 0
#include <winsock2.h>
#endif
#include <zlib.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET netsock;
typedef SOCKADDR netsockaddr;
typedef SOCKADDR_IN netsockaddr_in;
typedef int netsocklen_t;
typedef char sockopt_timeval;
typedef char netbuf;
#else
typedef socklen_t netsock_len;
typedef int netsock;
typedef struct sockaddr netsockaddr;
typedef struct sockaddr_in netsockaddr_in;
typedef socklen_t netsocklen_t;
typedef struct timeval sockopt_timeval;
typedef void netbuf;
#endif

#include "log.h"
//#include "tunnel_thread.h"

struct xfer_stats
{
    float bps;
    long double total_l;
    char units[5];
    char ticker[5];
    int ticker_i;
    time_t time_l;
};

struct Packet
{
    unsigned char *data;
    int id;
    int type;
    char src[16];
    char dst[16];
    int len;
};
struct crypt
{
    unsigned char *data;
    int len;
};
struct compression
{
    unsigned char *data;
    int len;
};
struct keys
{
    int version;
    unsigned char key[18];
    unsigned char iv[18];
    char ciphername[255];
#ifdef HAVE_OPENSSL
    const EVP_CIPHER *cipher;
#else
    int cipher;
    int mode;
#endif
};
struct Host
{
    char *ip;
    int ps;
    int pe;
};
struct options
{
    //    char host[255];
    int vpn;
    int port_listen;
    int port_forward;
    int listen;
    int forward;
    int role;
    int daemon;
    int proto;
    int packet_size;
    int comp;
    int comp_level;
    int tun;
    int id;
    int proxy;
    int printkey;
    int vmode;
    int stats;
    struct keys key;
    struct Host local;
    struct Host remote;
    char pool[32];
    char gw[32];
    char cipher[254];
    char *networks;
    char *keyfile;
    char *pexec;
    char *ppp;
#ifndef HAVE_OPENSSL
    char mode[25];
#endif
};
struct Network_timer
{
    clock_t start;
    clock_t now;
    float bps;
    float total;
};
struct Network_rate
{
    struct Network_timer tx;
    struct Network_timer rx;
};
struct Network
{
    netsockaddr_in addr;
    netsock sockfd;
    netsocklen_t len;
    struct Network_rate rate;
};
struct Compress
{
    z_stream deflate;
    z_stream inflate;
};
#ifdef HAVE_OPENSSL
typedef EVP_CIPHER_CTX crypto_ctx;
#else
typedef gcry_cipher_hd_t crypto_ctx;
#endif

/*
struct Network_ent {
    unsigned long i;
    unsigned long m;
};
*/
struct Networks
{
    unsigned long dst;
    unsigned long msk;
};
/*
struct Networks {
    in_addr_t dst;
    in_addr_t msk;
};
*/
struct Ctunnel
{
#ifdef HAVE_OPENSSL
    crypto_ctx *ectx;
    crypto_ctx *dctx;
#else
    crypto_ctx ectx;
    crypto_ctx dctx;
#endif
    struct Network *net_cli;
    struct Network *net_srv;
    //    struct Networks nets[32];
    int nets_t;
    struct Compress comp;
    struct options opt;
    int srv_sockfd;
    int clisockfd;
    int srvsockfd;
    int tunfd;
    int id;
    time_t seen;
    char src[32];
    char dst[32];
};

pthread_mutex_t mutex;

#define KEYFILE "/.passkey"
#define PACKET_SIZE 2048
#define MAX_THREADS 2048
#define MAX_CLIENTS 10

int threads[MAX_THREADS];

//char **hosts;
struct Hosts
{
    struct Network *net;
    struct Networks n[254];
    char natip[32];
    char tunip[32];
    char hostname[255];
    int id;
    int ni;
    struct in_addr addr;
    time_t time;
};
struct Hosts hosts[MAX_CLIENTS];
int clients;

#ifdef HAVE_OPENSSL
struct Packet (*do_encrypt)(crypto_ctx *ctx, unsigned char *intext, int size);
struct Packet (*do_decrypt)(crypto_ctx *ctx, unsigned char *intext, int size);
crypto_ctx *(*crypto_init)(struct options opt, int dir);
void (*crypto_deinit)(crypto_ctx *ctx);
void (*crypto_reset)(crypto_ctx *ctx, struct options opt, int dir);
#else
struct Packet (*do_encrypt)(crypto_ctx ctx, unsigned char *intext, int size);
struct Packet (*do_decrypt)(crypto_ctx ctx, unsigned char *intext, int size);
crypto_ctx (*crypto_init)(struct options opt, int dir);
void (*crypto_deinit)(crypto_ctx ctx);
void (*crypto_reset)(crypto_ctx ctx, struct options opt, int dir);
#endif

#define SERVER 1
#define CLIENT 0
#define TCP 1
#define UDP 0
#define VMODE_TUN 0
#define VMODE_PPP 1
#define PACKET_CONRQ 0x01 /* Connection request */
#define PACKET_DATA 0x02  /* Raw Data */
#define PACKET_PING 0x03  /* Hello */
#define PACKET_ROUTE 0x04 /* Routes follow */
#define PACKET_HOSTN 0x05 /* Hostname */
#define PACKET_CLNTR 0x06 /* Client Address response */
#define PACKET_NRTE 0x07  /* No routes */
#define PACKET_REUP 0x08  /* Request renegotiation */

#include "config.h"
#include "comp.h"
#include "crypto.h"
#include "stats.h"
#include "net.h"
#include "opt.h"
#include "tun.h"
#include "jtok.h"
#include "vpn_handshake.h"
#include "vpn_loop.h"
#include "vpn_thread.h"
#include "tunnel_thread.h"
#include "tunnel_loop.h"
#include "exec.h"
#include "keyfile.h"
