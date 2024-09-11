#ifndef _LS_NET_H_
#define _LS_NET_H_

#include "ls_defs.h"

#define LS_NET_STREAM 1
#define LS_NET_DGRAM 2

#define LS_NET_PROTO_TCP 1
#define LS_NET_PROTO_UDP 2

#define LS_AF_UNSPEC 0
#define LS_AF_INET 2
#define LS_AF_INET6 23

#define LS_NET_SHUT_RECV 0
#define LS_NET_SHUT_SEND 1
#define LS_NET_SHUT_BOTH 2

#define LS_NET_MAXCONN 0x7fffffff

ls_handle ls_net_connect(const char *host, unsigned short port, int type, int protocol, int addr_family);

ls_handle ls_net_listen(const char *host, unsigned short port, int type, int protocol, int addr_family, int backlog);

ls_handle ls_net_accept(ls_handle sock);

int ls_net_shutdown(ls_handle sock, int how);

const char *ls_net_gethost(ls_handle sock);

unsigned short ls_net_getport(ls_handle sock);

size_t ls_net_recv(ls_handle sock, void *buffer, size_t size);

size_t ls_net_send(ls_handle sock, const void *buffer, size_t size);

#endif // _LS_NET_H_
