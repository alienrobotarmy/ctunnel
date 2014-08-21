/* tun.c */
int add_routes(struct Ctunnel *ct, char *gw, char *dev, int id, char *networks);
int route_add(char *dev, char *gw, char *dst, char *msk);
int mktun(char *dev);
int ifconfig(char *dev, char *gw, char *ip, char *netmask);
