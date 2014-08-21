/* src/net.c */
int in_subnet(char *net, char *host, char *mask);
int cidr_to_mask(char *data, int cidr);
char *net_resolv(char *ip);
struct Network *udp_bind(char *ip, int port, int timeout);
struct Network *udp_connect(char *ip, int port, int timeout);
netsock tcp_listen(char *ip, int port);
netsock tcp_accept(int sockfd);
netsock tcp_connect(char *ip, int port);
int net_read(struct Ctunnel *ct, struct Network *net, unsigned char *data, int len, int enc);
int net_write(struct Ctunnel *ct, struct Network *net, unsigned char *data, int len, int enc);
void net_close(struct Network *net);
